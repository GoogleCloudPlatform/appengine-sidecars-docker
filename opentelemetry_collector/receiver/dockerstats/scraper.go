package dockerstats

import (
	"context"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"strings"
	"time"

	"github.com/docker/docker/api/types"
	"github.com/docker/docker/client"
	"github.com/golang/glog"

	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/consumer/consumerdata"
	"go.opentelemetry.io/collector/consumer/pdatautil"

	mpb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"

	"github.com/googlecloudplatform/appengine-sidecars-docker/opentelemetry_collector/receiver/metricgenerator"
)

var (
	containerNameLabel = &mpb.LabelKey{
		Key:         "container_name",
		Description: "Name of the container (or ID if name is not available)",
	}

	memUsageDesc = &mpb.MetricDescriptor{
		Name:        "container/memory/usage",
		Description: "Total memory the container is using",
		Unit:        "By",
		Type:        mpb.MetricDescriptor_GAUGE_INT64,
		LabelKeys:   []*mpb.LabelKey{containerNameLabel},
	}
	memLimitDesc = &mpb.MetricDescriptor{
		Name:        "container/memory/limit",
		Description: "Total memory the container is allowed to use",
		Unit:        "By",
		Type:        mpb.MetricDescriptor_GAUGE_INT64,
		LabelKeys:   []*mpb.LabelKey{containerNameLabel},
	}
)

type scraper struct {
	startTime      time.Time
	scrapeInterval time.Duration
	done           chan bool

	metricConsumer consumer.MetricsConsumer
	docker         client.ContainerAPIClient
}

func newScraper(scrapeInterval time.Duration, metricConsumer consumer.MetricsConsumer) (*scraper, error) {
	docker, err := client.NewEnvClient()
	if err != nil {
		return nil, fmt.Errorf("failed to initialize docker client: %v", err)
	}

	return &scraper{
		scrapeInterval: scrapeInterval,
		done:           make(chan bool),
		metricConsumer: metricConsumer,
		docker:         docker,
	}, nil
}

func (s *scraper) start() {
	s.startTime = time.Now()
	go func() {
		ticker := time.NewTicker(s.scrapeInterval)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				s.export()
			case <-s.done:
				return
			}
		}
	}()
}

func (s *scraper) stop() {
	s.done <- true
}

func (s *scraper) export() {
	ctx, cancel := context.WithTimeout(context.Background(), s.scrapeInterval)
	defer cancel()

	containers, err := s.docker.ContainerList(ctx, types.ContainerListOptions{})
	if err != nil {
		glog.Warningf("Failed to get docker container list: %v", err)
		return
	}

	var metrics []*mpb.Metric
	for _, container := range containers {
		var name string
		if len(container.Names) > 0 {
			name = strings.TrimPrefix(container.Names[0], "/")
		} else {
			name = container.ID
		}
		labelValues := []*mpb.LabelValue{metricgenerator.MakeLabelValue(name)}

		stats, err := s.readStats(ctx, container.ID)
		if err != nil {
			glog.Warningf("readStats failed for container %s: %v", container.ID, err)
			continue
		}

		metrics = append(metrics, &mpb.Metric{
			MetricDescriptor: memUsageDesc,
			Timeseries: []*mpb.TimeSeries{
				metricgenerator.MakeInt64TimeSeries(int64(stats.MemoryStats.Usage), s.startTime, time.Now(), labelValues),
			},
		})
		metrics = append(metrics, &mpb.Metric{
			MetricDescriptor: memLimitDesc,
			Timeseries: []*mpb.TimeSeries{
				metricgenerator.MakeInt64TimeSeries(int64(stats.MemoryStats.Limit), s.startTime, time.Now(), labelValues),
			},
		})
	}

	md := consumerdata.MetricsData{Metrics: metrics}
	s.metricConsumer.ConsumeMetrics(ctx, pdatautil.MetricsFromMetricsData([]consumerdata.MetricsData{md}))
}

func (s *scraper) readStats(ctx context.Context, id string) (*types.Stats, error) {
	st, err := s.docker.ContainerStats(ctx, id, false /*stream*/)
	if err != nil {
		return nil, fmt.Errorf("failed to retrieve stats: %v", err)
	}
	defer st.Body.Close()

	b, err := ioutil.ReadAll(st.Body)
	if err != nil {
		return nil, fmt.Errorf("failed to read stats: %v", err)
	}

	var stats types.Stats
	if err = json.Unmarshal(b, &stats); err != nil {
		return nil, fmt.Errorf("failed to unmarshal stats JSON: %v", err)
	}
	return &stats, nil
}
