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
	"github.com/docker/docker/client"
	"github.com/stretchr/testify/assert"

	"go.opentelemetry.io/collector/consumer/consumerdata"
	"go.opentelemetry.io/collector/consumer/pdata"
	"go.opentelemetry.io/collector/consumer/pdatautil"

	mpb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"
)

type fakeDocker struct {
	client.Client
}

func (d *fakeDocker) ContainerList(ctx context.Context, opts types.ContainerListOptions) ([]types.Container, error) {
	return []types.Container{
		types.Container{
			ID:    "id1",
			Names: []string{"name1a", "name1b"},
		},
		types.Container{
			ID:    "id2",
			Names: []string{},
		},
	}, nil
}

func (d *fakeDocker) ContainerStats(ctx context.Context, id string, stream bool) (types.ContainerStats, error) {
	s1 := types.Stats{
		MemoryStats: types.MemoryStats{
			Usage: 33,
			Limit: 66,
		},
	}
	s2 := types.Stats{
		MemoryStats: types.MemoryStats{
			Usage: 44,
			Limit: 88,
		},
	}
	var stats types.Stats
	switch id {
	case "id1":
		stats = s1
	case "id2":
		stats = s2
	}

	b, err := json.Marshal(stats)
	if err != nil {
		return types.ContainerStats{}, fmt.Errorf("failed to marshal JSON: %v", err)
	}

	return types.ContainerStats{
		Body: ioutil.NopCloser(bytes.NewReader(b)),
	}, nil
}

// fakeMetricConsumer extends consumer.MetricsConsumer.
type fakeMetricsConsumer struct {
	metrics pdata.Metrics
}

func (c *fakeMetricsConsumer) ConsumeMetrics(ctx context.Context, md pdata.Metrics) error {
	c.metrics = md
	return nil
}

func TestScraperExport(t *testing.T) {
	now := time.Now()
	c := &fakeMetricsConsumer{}
	s := &scraper{
		startTime:      now,
		metricConsumer: c,
		docker:         &fakeDocker{},
		scrapeInterval: 10 * time.Second,
	}

	s.export()

	data := pdatautil.MetricsToMetricsData(c.metrics)[0]
	assert.Len(t, data.Metrics, 4)
	verifyContainerMetricValue(t, data, "container/memory/usage", "name1a", 33)
	verifyContainerMetricValue(t, data, "container/memory/limit", "name1a", 66)
	verifyContainerMetricValue(t, data, "container/memory/usage", "id2", 44)
	verifyContainerMetricValue(t, data, "container/memory/limit", "id2", 88)
}

func verifyContainerMetricValue(t *testing.T, data consumerdata.MetricsData, name, label string, value int64) {
	var metric *mpb.Metric
	for _, m := range data.Metrics {
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
