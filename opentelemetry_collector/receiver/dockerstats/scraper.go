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

	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/consumer/consumerdata"
	"go.opentelemetry.io/collector/translator/internaldata"
	"go.uber.org/zap"

	mpb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"

	"github.com/googlecloudplatform/appengine-sidecars-docker/opentelemetry_collector/receiver/metricgenerator"
)

var (
	containerNameLabel = &mpb.LabelKey{
		Key:         "container_name",
		Description: "Name of the container (or ID if name is not available)",
	}

	cpuUsageDesc = &mpb.MetricDescriptor{
		Name:        "container/cpu/usage_time",
		Description: "Total CPU time consumed",
		Unit:        "seconds",
		Type:        mpb.MetricDescriptor_CUMULATIVE_DOUBLE,
		LabelKeys:   []*mpb.LabelKey{containerNameLabel},
	}
	cpuLimitDesc = &mpb.MetricDescriptor{
		Name:        "container/cpu/limit",
		Description: "CPU time limit (where applicable)",
		Unit:        "seconds",
		Type:        mpb.MetricDescriptor_GAUGE_DOUBLE,
		LabelKeys:   []*mpb.LabelKey{containerNameLabel},
	}
	memUsageDesc = &mpb.MetricDescriptor{
		Name:        "container/memory/usage",
		Description: "Total memory the container is using",
		Unit:        "bytes",
		Type:        mpb.MetricDescriptor_GAUGE_INT64,
		LabelKeys:   []*mpb.LabelKey{containerNameLabel},
	}
	memLimitDesc = &mpb.MetricDescriptor{
		Name:        "container/memory/limit",
		Description: "Total memory the container is allowed to use",
		Unit:        "bytes",
		Type:        mpb.MetricDescriptor_GAUGE_INT64,
		LabelKeys:   []*mpb.LabelKey{containerNameLabel},
	}
	nwRecvBytesDesc = &mpb.MetricDescriptor{
		Name:        "container/network/received_bytes_count",
		Description: "Bytes received by container over all network interfaces",
		Unit:        "bytes",
		Type:        mpb.MetricDescriptor_CUMULATIVE_INT64,
		LabelKeys:   []*mpb.LabelKey{containerNameLabel},
	}
	nwSentBytesDesc = &mpb.MetricDescriptor{
		Name:        "container/network/sent_bytes_count",
		Description: "Bytes sent by container over all network interfaces",
		Unit:        "byte",
		Type:        mpb.MetricDescriptor_CUMULATIVE_INT64,
		LabelKeys:   []*mpb.LabelKey{containerNameLabel},
	}
	// Container health metrics.
	uptimeDesc = &mpb.MetricDescriptor{
		Name:        "container/uptime",
		Description: "Container uptime",
		Unit:        "seconds",
		Type:        mpb.MetricDescriptor_GAUGE_INT64,
		LabelKeys:   []*mpb.LabelKey{containerNameLabel},
	}
	restartCountDesc = &mpb.MetricDescriptor{
		Name:        "container/restart_count",
		Description: "Number of times the container has been restarted.",
		Unit:        "Count",
		Type:        mpb.MetricDescriptor_CUMULATIVE_INT64,
		LabelKeys:   []*mpb.LabelKey{containerNameLabel},
	}
)

type containerInfo struct {
	uptime       time.Duration
	restartCount int64
	cpuLimit     int64
}

type scraper struct {
	startTime      time.Time
	scrapeInterval time.Duration
	done           chan bool
	scrapeCount    uint64

	metricConsumer consumer.MetricsConsumer
	docker         client.ContainerAPIClient
	logger         *zap.Logger

	now func() time.Time
}

func newScraper(scrapeInterval time.Duration, metricConsumer consumer.MetricsConsumer, logger *zap.Logger) (*scraper, error) {
	docker, err := client.NewEnvClient()
	if err != nil {
		return nil, fmt.Errorf("failed to initialize docker client: %v", err)
	}

	return &scraper{
		scrapeInterval: scrapeInterval,
		done:           make(chan bool),
		metricConsumer: metricConsumer,
		docker:         docker,
		logger:         logger,
		now:            time.Now,
	}, nil
}

func (s *scraper) start() {
	s.startTime = s.now()
	go func() {
		ticker := time.NewTicker(s.scrapeInterval)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				s.export()
				s.scrapeCount++
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
		s.logger.Warn("Failed to get docker container list.", zap.Error(err))
		return
	}

	var metrics []*mpb.Metric
	for _, container := range containers {
		var name string
		if len(container.Names) > 0 {
			// Docker container names are prefixed with their parent's name (/ means docker
			// daemon). See https://github.com/moby/moby/issues/6705#issuecomment-47298276.
			name = strings.TrimPrefix(container.Names[0], "/")
		} else {
			name = container.ID
		}
		labelValues := []*mpb.LabelValue{metricgenerator.MakeLabelValue(name)}
		cLogger := s.logger.With(zap.String("name", name), zap.String("id", container.ID))

		stats, err := s.readResourceUsageStats(ctx, container.ID)
		if err != nil {
			cLogger.Warn("readResourceUsageStats failed.", zap.Error(err))
		} else {
			metrics = append(metrics, s.usageStatsToMetrics(stats, labelValues)...)
		}

		info, err := s.readContainerInfo(ctx, container.ID)
		if err != nil {
			cLogger.Warn("readContainerInfo failed.", zap.Error(err))
		} else {
			metrics = append(metrics, s.containerInfoToMetrics(info, labelValues)...)
		}
	}

	md := consumerdata.MetricsData{Metrics: metrics}
	err = s.metricConsumer.ConsumeMetrics(ctx, internaldata.OCSliceToMetrics([]consumerdata.MetricsData{md}))
	if err != nil {
		s.logger.Error("Error sending docker stats metrics", zap.Error(err))
	}
}

func (s *scraper) readResourceUsageStats(ctx context.Context, id string) (*types.StatsJSON, error) {
	st, err := s.docker.ContainerStats(ctx, id, false /*stream*/)
	if err != nil {
		return nil, fmt.Errorf("failed to retrieve stats: %v", err)
	}
	defer st.Body.Close()

	b, err := ioutil.ReadAll(st.Body)
	if err != nil {
		return nil, fmt.Errorf("failed to read stats: %v", err)
	}

	var stats types.StatsJSON
	if err = json.Unmarshal(b, &stats); err != nil {
		return nil, fmt.Errorf("failed to unmarshal stats JSON: %v", err)
	}
	return &stats, nil
}

func (s *scraper) usageStatsToMetrics(stats *types.StatsJSON, labelValues []*mpb.LabelValue) []*mpb.Metric {
	var rx, tx uint64
	for _, nw := range stats.Networks {
		rx += nw.RxBytes
		tx += nw.TxBytes
	}

	return []*mpb.Metric{
		{
			MetricDescriptor: cpuUsageDesc,
			Timeseries: []*mpb.TimeSeries{
				metricgenerator.MakeDoubleTimeSeries(time.Duration(stats.CPUStats.CPUUsage.TotalUsage).Seconds(), s.startTime, s.now(), labelValues),
			},
		},
		// Unfortunately, Docker API doesn't expose CPU Limits via CPUStats API. That information
		// is extracted via container inspection (see readInfo() below).
		{
			MetricDescriptor: memUsageDesc,
			Timeseries: []*mpb.TimeSeries{
				metricgenerator.MakeInt64TimeSeries(int64(stats.MemoryStats.Usage), s.startTime, s.now(), labelValues),
			},
		},
		{
			MetricDescriptor: memLimitDesc,
			Timeseries: []*mpb.TimeSeries{
				metricgenerator.MakeInt64TimeSeries(int64(stats.MemoryStats.Limit), s.startTime, s.now(), labelValues),
			},
		},
		{
			MetricDescriptor: nwRecvBytesDesc,
			Timeseries: []*mpb.TimeSeries{
				metricgenerator.MakeInt64TimeSeries(int64(rx), s.startTime, s.now(), labelValues),
			},
		},
		{
			MetricDescriptor: nwSentBytesDesc,
			Timeseries: []*mpb.TimeSeries{
				metricgenerator.MakeInt64TimeSeries(int64(tx), s.startTime, s.now(), labelValues),
			},
		},
	}
}

func (s *scraper) readContainerInfo(ctx context.Context, id string) (containerInfo, error) {
	var info containerInfo

	c, err := s.docker.ContainerInspect(ctx, id)
	if err != nil {
		return info, fmt.Errorf("failed to retrieve container info: %v", err)
	}
	info.restartCount = int64(c.RestartCount)
	info.cpuLimit = c.HostConfig.NanoCPUs

	t, err := time.Parse(time.RFC3339Nano, c.State.StartedAt)
	if err != nil {
		return info, fmt.Errorf("failed to parse container start time (%s): %v", c.State.StartedAt, err)
	}
	now := s.now()
	if t.After(now) {
		return info, fmt.Errorf("invalid container start time %v, should be <= current time %v", t, now)
	}
	info.uptime = now.Sub(t)

	return info, nil
}

func (s *scraper) containerInfoToMetrics(info containerInfo, labelValues []*mpb.LabelValue) []*mpb.Metric {
	metrics := []*mpb.Metric{
		{
			MetricDescriptor: uptimeDesc,
			Timeseries: []*mpb.TimeSeries{
				metricgenerator.MakeInt64TimeSeries(int64(info.uptime.Seconds()), s.startTime, s.now(), labelValues),
			},
		},
		{
			MetricDescriptor: restartCountDesc,
			Timeseries: []*mpb.TimeSeries{
				metricgenerator.MakeInt64TimeSeries(info.restartCount, s.startTime, s.now(), labelValues),
			},
		},
	}
	if info.cpuLimit > 0 { // only generate if the container has a CPU limit.
		metrics = append(metrics, &mpb.Metric{
			MetricDescriptor: cpuLimitDesc,
			Timeseries: []*mpb.TimeSeries{
				metricgenerator.MakeDoubleTimeSeries(time.Duration(info.cpuLimit).Seconds(), s.startTime, s.now(), labelValues),
			},
		})
	}
	return metrics
}
