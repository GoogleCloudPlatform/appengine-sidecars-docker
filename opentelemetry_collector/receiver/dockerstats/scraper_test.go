package dockerstats

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"testing"
	"time"

	"github.com/docker/docker/api/types"
	"github.com/docker/docker/api/types/container"
	"github.com/docker/docker/client"

	"github.com/stretchr/testify/assert"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/model/pdata"
	"go.uber.org/zap"

	mpb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"
	"github.com/open-telemetry/opentelemetry-collector-contrib/pkg/translator/opencensus"
)

type fakeDocker struct {
	client.Client
}

func (d *fakeDocker) ContainerList(ctx context.Context, opts types.ContainerListOptions) ([]types.Container, error) {
	return []types.Container{
		{
			ID:    "id1",
			Names: []string{"name1a", "name1b"},
		},
		{
			ID:    "id2",
			Names: []string{},
		},
		{
			ID:    "id3",
			Names: []string{"name3"},
		},
	}, nil
}

func (d *fakeDocker) ContainerStats(ctx context.Context, id string, stream bool) (types.ContainerStats, error) {
	s1 := types.StatsJSON{
		Stats: types.Stats{
			CPUStats: types.CPUStats{
				CPUUsage: types.CPUUsage{
					TotalUsage: 100000000,
				},
			},
			MemoryStats: types.MemoryStats{
				Usage: 33,
				Limit: 66,
			},
		},
		Networks: map[string]types.NetworkStats{
			"eth0": {
				RxBytes: 111,
				TxBytes: 222,
			},
		},
	}
	s2 := types.StatsJSON{
		Stats: types.Stats{
			CPUStats: types.CPUStats{
				CPUUsage: types.CPUUsage{
					TotalUsage: 200000000,
				},
			},
			MemoryStats: types.MemoryStats{
				Usage: 44,
				Limit: 88,
			},
		},
		Networks: map[string]types.NetworkStats{
			"eth0": {
				RxBytes: 333,
				TxBytes: 444,
			},
			"eth1": {
				RxBytes: 222,
				TxBytes: 333,
			},
		},
	}
	s3 := types.StatsJSON{}

	var stats types.StatsJSON
	var err error
	switch id {
	case "id1":
		stats = s1
	case "id2":
		stats = s2
	case "id3":
		stats = s3
		err = fmt.Errorf("manual failure")
	}

	b, err2 := json.Marshal(stats)
	if err2 != nil {
		return types.ContainerStats{}, fmt.Errorf("failed to marshal JSON: %v", err2)
	}

	return types.ContainerStats{
		Body: ioutil.NopCloser(bytes.NewReader(b)),
	}, err
}

func (d *fakeDocker) ContainerInspect(ctx context.Context, id string) (types.ContainerJSON, error) {
	var c types.ContainerJSON
	var err error

	switch id {
	case "id1":
		c = types.ContainerJSON{
			ContainerJSONBase: &types.ContainerJSONBase{
				RestartCount: 3,
				State: &types.ContainerState{
					StartedAt: "2019-12-31T12:00:00.000000000Z",
				},
				HostConfig: &container.HostConfig{},
			},
		}
	case "id2":
		c = types.ContainerJSON{
			ContainerJSONBase: &types.ContainerJSONBase{
				RestartCount: 5,
				State: &types.ContainerState{
					StartedAt: "2019-12-31T00:00:00.000000000Z",
				},
				HostConfig: &container.HostConfig{
					Resources: container.Resources{
						NanoCPUs: 500000000,
					},
				},
			},
		}
	case "id3":
		c = types.ContainerJSON{}
		err = fmt.Errorf("manual error")
	}

	return c, err
}

// fakeMetricConsumer extends consumer.MetricsConsumer.
type fakeMetricsConsumer struct {
	metrics pdata.Metrics
}

func (c *fakeMetricsConsumer) Capabilities() consumer.Capabilities {
	return consumer.Capabilities{
		MutatesData: false,
	}
}

func (c *fakeMetricsConsumer) ConsumeMetrics(ctx context.Context, md pdata.Metrics) error {
	c.metrics = md
	return nil
}

func fakeNow() time.Time {
	t, _ := time.Parse(time.RFC3339, "2020-01-01T00:00:00Z")
	return t
}

func TestScraperExport(t *testing.T) {
	c := &fakeMetricsConsumer{}
	s := &scraper{
		startTime:      fakeNow(),
		metricConsumer: c,
		docker:         &fakeDocker{},
		scrapeInterval: 10 * time.Second,
		now:            fakeNow,
		logger:         zap.NewNop(),
	}

	s.export()

	_, _, data := opencensus.ResourceMetricsToOC(c.metrics.ResourceMetrics().At(0))
	verifyContainerMetricDoubleValue(t, data, "container/cpu/usage_time", "name1a", 0.1)
	verifyContainerMetricAbsent(t, data, "container/cpu/limit", "name1a")
	verifyContainerMetricInt64Value(t, data, "container/memory/usage", "name1a", 33)
	verifyContainerMetricInt64Value(t, data, "container/memory/limit", "name1a", 66)
	verifyContainerMetricInt64Value(t, data, "container/network/received_bytes_count", "name1a", 111)
	verifyContainerMetricInt64Value(t, data, "container/network/sent_bytes_count", "name1a", 222)
	verifyContainerMetricInt64Value(t, data, "container/uptime", "name1a", 43200)
	verifyContainerMetricInt64Value(t, data, "container/restart_count", "name1a", 3)
	verifyContainerMetricDoubleValue(t, data, "container/cpu/usage_time", "id2", 0.2)
	verifyContainerMetricDoubleValue(t, data, "container/cpu/limit", "id2", 0.5)
	verifyContainerMetricInt64Value(t, data, "container/memory/usage", "id2", 44)
	verifyContainerMetricInt64Value(t, data, "container/memory/limit", "id2", 88)
	verifyContainerMetricInt64Value(t, data, "container/network/received_bytes_count", "id2", 555)
	verifyContainerMetricInt64Value(t, data, "container/network/sent_bytes_count", "id2", 777)
	verifyContainerMetricInt64Value(t, data, "container/uptime", "id2", 86400)
	verifyContainerMetricInt64Value(t, data, "container/restart_count", "id2", 5)
	verifyContainerMetricAbsent(t, data, "container/cpu/usage_time", "name3")
	verifyContainerMetricAbsent(t, data, "container/cpu/limit", "name3")
	verifyContainerMetricAbsent(t, data, "container/memory/usage", "name3")
	verifyContainerMetricAbsent(t, data, "container/memory/limit", "name3")
	verifyContainerMetricAbsent(t, data, "container/network/received_bytes_count", "name3")
	verifyContainerMetricAbsent(t, data, "container/network/sent_bytes_count", "name3")
	verifyContainerMetricAbsent(t, data, "container/uptime", "name3")
	verifyContainerMetricAbsent(t, data, "container/restart_count", "name3")
}

func verifyContainerMetricInt64Value(t *testing.T, data []*mpb.Metric, name, label string, value int64) {
	var metric *mpb.Metric
	for _, m := range data {
		if m.MetricDescriptor.Name == name && m.Timeseries[0].LabelValues[0].Value == label {
			metric = m
		}
	}
	if metric == nil {
		t.Errorf("Unable to find metric %q", name)
		return
	}
	assert.Equal(t, value, metric.Timeseries[0].Points[0].GetInt64Value())
}

func verifyContainerMetricDoubleValue(t *testing.T, data []*mpb.Metric, name, label string, value float64) {
	var metric *mpb.Metric
	for _, m := range data {
		if m.MetricDescriptor.Name == name && m.Timeseries[0].LabelValues[0].Value == label {
			metric = m
		}
	}
	if metric == nil {
		t.Errorf("Unable to find metric %q", name)
		return
	}
	assert.Equal(t, value, metric.Timeseries[0].Points[0].GetDoubleValue())
}

func verifyContainerMetricAbsent(t *testing.T, data []*mpb.Metric, name, label string) {
	for _, m := range data {
		if m.MetricDescriptor.Name == name && m.Timeseries[0].LabelValues[0].Value == label {
			t.Errorf("Expected metric %s{container_name=%s} to be absent, found metric: %v", name, label, m)
			break
		}
	}
}

type alwaysFailDocker struct {
	client.Client
}

func (d *alwaysFailDocker) ContainerList(_ context.Context, _ types.ContainerListOptions) ([]types.Container, error) {
	return []types.Container{}, fmt.Errorf("always fail")
}

func TestScraperContinuesOnError(t *testing.T) {
	s := &scraper{
		now:            fakeNow,
		docker:         &alwaysFailDocker{},
		scrapeInterval: 1 * time.Second,
		done:           make(chan bool),
		logger:         zap.NewNop(),
	}
	s.start()
	time.Sleep(6 * time.Second)
	s.stop()
	assert.GreaterOrEqual(t, s.scrapeCount, uint64(5))
}
