package nginxreceiver

import (
	"bytes"
	"context"
	"errors"
	"io/ioutil"
	"net/http"
	"testing"
	"time"

	metricspb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"

	"github.com/stretchr/testify/assert"
	"go.opentelemetry.io/collector/consumer/consumerdata"
	"go.opentelemetry.io/collector/consumer/pdata"
	"go.opentelemetry.io/collector/consumer/pdatautil"
	"go.uber.org/zap"

	"github.com/googlecloudplatform/appengine-sidecars-docker/opentelemetry_collector/receiver/metricgenerator"
)

func fakeNow() time.Time {
	t, _ := time.Parse(time.RFC3339, "2020-01-01T00:00:00Z")
	return t
}

func fakeHTTPGet(testURL string) (resp *http.Response, err error) {
	successJSON := `{
  "accepted_connections": 3,
  "handled_connections": 3,
  "active_connections": 1,
  "requests": 3,
  "reading_connections": 0,
  "writing_connections": 1,
  "waiting_connections": 0,
  "request_latency":{
    "latency_sum": 8,
    "request_count": 3,
    "sum_squares": 24,
    "distribution": [0, 2, 1]
  },
  "upstream_latency":{
    "latency_sum": 5,
    "request_count": 3,
    "sum_squares": 9,
    "distribution": [1, 2, 0]
  },
  "websocket_latency":{
    "latency_sum": 4,
    "request_count": 1,
    "sum_squares": 16,
    "distribution": [0, 0, 1]
  },
  "latency_bucket_bounds": [2, 4]
}`
	malformattedJSON := "malformatted json requests 0"
	if testURL == "http://success" {
		return getResponseFromJSON(successJSON, 200), nil
	} else if testURL == "http://not_found" {
		return getResponseFromJSON("{}", 404), nil
	} else if testURL == "http://malformatted" {
		return getResponseFromJSON(malformattedJSON, 200), nil
	} else if testURL == "http://unset" {
		return getResponseFromJSON("{}", 200), nil
	}
	return nil, errors.New("failed request")
}

func getResponseFromJSON(json string, status int) *http.Response {
	body := ioutil.NopCloser(bytes.NewReader([]byte(json)))
	return &http.Response{
		StatusCode: status,
		Body:       body,
	}
}

type fakeConsumer struct {
	metrics pdata.Metrics
}

func (consumer *fakeConsumer) ConsumeMetrics(ctx context.Context, metrics pdata.Metrics) error {
	consumer.metrics = metrics
	return nil
}

func TestScrapeNginxStats(t *testing.T) {
	collector := &NginxStatsCollector{
		consumer:       &fakeConsumer{},
		now:            fakeNow,
		startTime:      fakeNow(),
		done:           make(chan struct{}),
		logger:         zap.NewNop(),
		exportInterval: time.Minute,
		statsURL:       "http://success",
		getStatus:      fakeHTTPGet,
	}

	stats, err := collector.scrapeNginxStats()

	expectedStats := &NginxStats{
		RequestLatency: LatencyStats{
			RequestCount: 3,
			LatencySum:   8,
			SumSquares:   24,
			Distribution: []int64{0, 2, 1},
		},
		UpstreamLatency: LatencyStats{
			RequestCount: 3,
			LatencySum:   5,
			SumSquares:   9,
			Distribution: []int64{1, 2, 0},
		},
		WebsocketLatency: LatencyStats{
			RequestCount: 1,
			LatencySum:   4,
			SumSquares:   16,
			Distribution: []int64{0, 0, 1},
		},
		LatencyBucketBounds: []float64{2, 4},
	}
	assert.Nil(t, err)
	assert.Equal(t, expectedStats, stats)
}

func TestScrapeNginxStatsUnset(t *testing.T) {
	collector := &NginxStatsCollector{
		consumer:       &fakeConsumer{},
		now:            fakeNow,
		startTime:      fakeNow(),
		done:           make(chan struct{}),
		logger:         zap.NewNop(),
		exportInterval: time.Minute,
		statsURL:       "http://unset",
		getStatus:      fakeHTTPGet,
	}

	stats, err := collector.scrapeNginxStats()

	expectedStats := &NginxStats{
		RequestLatency: LatencyStats{
			RequestCount: -1,
			LatencySum:   -1,
			SumSquares:   -1,
			Distribution: nil,
		},
		UpstreamLatency: LatencyStats{
			RequestCount: -1,
			LatencySum:   -1,
			SumSquares:   -1,
			Distribution: nil,
		},
		WebsocketLatency: LatencyStats{
			RequestCount: -1,
			LatencySum:   -1,
			SumSquares:   -1,
			Distribution: nil,
		},
		LatencyBucketBounds: nil,
	}
	assert.Nil(t, err)
	assert.Equal(t, expectedStats, stats)
}

func TestScrapeNginxStatsNotFound(t *testing.T) {
	collector := &NginxStatsCollector{
		consumer:       &fakeConsumer{},
		startTime:      fakeNow(),
		now:            fakeNow,
		done:           make(chan struct{}),
		logger:         zap.NewNop(),
		exportInterval: time.Minute,
		statsURL:       "http://not_found",
		getStatus:      fakeHTTPGet,
	}

	_, err := collector.scrapeNginxStats()

	assert.NotNil(t, err)
}

func TestScrapeNginxStatsMalformatted(t *testing.T) {
	collector := &NginxStatsCollector{
		consumer:       &fakeConsumer{},
		startTime:      fakeNow(),
		now:            fakeNow,
		done:           make(chan struct{}),
		logger:         zap.NewNop(),
		exportInterval: time.Minute,
		statsURL:       "http://malformatted",
		getStatus:      fakeHTTPGet,
	}

	_, err := collector.scrapeNginxStats()

	assert.NotNil(t, err)
}

func TestScrapeNginxStatsError(t *testing.T) {
	collector := &NginxStatsCollector{
		consumer:       &fakeConsumer{},
		startTime:      fakeNow(),
		now:            fakeNow,
		done:           make(chan struct{}),
		logger:         zap.NewNop(),
		exportInterval: time.Minute,
		statsURL:       "http://error",
		getStatus:      fakeHTTPGet,
	}

	_, err := collector.scrapeNginxStats()

	assert.NotNil(t, err)
}

func TestCheckConsistency(t *testing.T) {
	stats := LatencyStats{
		RequestCount: 3,
		LatencySum:   8,
		SumSquares:   24,
		Distribution: []int64{0, 2, 1},
	}
	buckets := []float64{2, 4}

	err := stats.checkConsistency(buckets)
	assert.Nil(t, err)
}

func TestCheckConsistencyDistributionLengthErr(t *testing.T) {
	stats := LatencyStats{
		RequestCount: 3,
		LatencySum:   8,
		SumSquares:   24,
		Distribution: []int64{0, 2, 1},
	}
	buckets := []float64{2, 4, 8}

	err := stats.checkConsistency(buckets)
	expectedError := errors.New("The length of the latency distribution and distribution bucket boundaries do not match")

	assert.Equal(t, expectedError, err)
}

func TestCheckConsistencyNegativeRequest(t *testing.T) {
	stats := LatencyStats{
		RequestCount: -1,
		LatencySum:   8,
		SumSquares:   24,
		Distribution: []int64{0, 2, 1},
	}
	buckets := []float64{2, 4}

	err := stats.checkConsistency(buckets)
	expectedError := errors.New("The request count is less than 0")
	assert.Equal(t, expectedError, err)
}

func TestCheckConsistencyNegativeSumSquares(t *testing.T) {
	stats := LatencyStats{
		RequestCount: 3,
		LatencySum:   8,
		SumSquares:   -1,
		Distribution: []int64{0, 2, 1},
	}
	buckets := []float64{2, 4}

	err := stats.checkConsistency(buckets)
	expectedError := errors.New("The sum of squared latencies is less than 0")
	assert.Equal(t, expectedError, err)
}

func TestCheckConsistencyNegativeSum(t *testing.T) {
	stats := LatencyStats{
		RequestCount: 3,
		LatencySum:   -1,
		SumSquares:   24,
		Distribution: []int64{0, 2, 1},
	}
	buckets := []float64{2, 4}

	err := stats.checkConsistency(buckets)
	expectedError := errors.New("The sum of latencies is less than 0")
	assert.Equal(t, expectedError, err)
}

func TestCheckConsistencyNegativeDistribution(t *testing.T) {
	stats := LatencyStats{
		RequestCount: 3,
		LatencySum:   8,
		SumSquares:   24,
		Distribution: []int64{0, 2, -1},
	}
	buckets := []float64{2, 4}

	err := stats.checkConsistency(buckets)
	expectedError := errors.New("One of the latency distribution counts is less than 0")
	assert.Equal(t, expectedError, err)
}

func TestCheckConsistencyUnsetDistribution(t *testing.T) {
	stats := LatencyStats{
		RequestCount: 3,
		LatencySum:   8,
		SumSquares:   24,
		Distribution: nil,
	}
	var buckets []float64

	err := stats.checkConsistency(buckets)
	expectedError := errors.New("One of the distribution values from the stats json is unset")
	assert.Equal(t, expectedError, err)
}

func TestAppendDistributionMetric(t *testing.T) {
	collector := &NginxStatsCollector{
		consumer:       &fakeConsumer{},
		now:            fakeNow,
		startTime:      fakeNow(),
		done:           make(chan struct{}),
		logger:         zap.NewNop(),
		exportInterval: time.Minute,
		statsURL:       "http://success",
		getStatus:      fakeHTTPGet,
	}
	stats := &LatencyStats{
		RequestCount: 3,
		LatencySum:   9,
		SumSquares:   33,
		Distribution: []int64{0, 2, 1},
	}
	metrics := []*metricspb.Metric{}
	bucketOptions := metricgenerator.FormatBucketOptions([]float64{2, 4})

	metrics = collector.appendDistributionMetric(
		stats,
		bucketOptions,
		metrics,
		requestLatencyMetric,
	)

	if assert.Len(t, metrics, 1) {
		expectedMetricDescriptor := &metricspb.MetricDescriptor{
			Name:        "on_vm_request_latencies",
			Description: "The request latency measured at nginx. Includes latency from nginx and the user's app code",
			Unit:        "milliseconds",
			Type:        metricspb.MetricDescriptor_CUMULATIVE_DISTRIBUTION,
			LabelKeys:   []*metricspb.LabelKey{},
		}
		expectedDistribution := &metricspb.DistributionValue{
			Count:                 3,
			Sum:                   9,
			SumOfSquaredDeviation: 6,
			BucketOptions:         bucketOptions,
			Buckets:               []*metricspb.DistributionValue_Bucket{{Count: 0}, {Count: 2}, {Count: 1}},
		}
		expectedPoint := &metricspb.Point{
			Timestamp: metricgenerator.TimeToTimestamp(fakeNow()),
			Value: &metricspb.Point_DistributionValue{
				DistributionValue: expectedDistribution,
			},
		}
		expectedTimeseries := &metricspb.TimeSeries{
			StartTimestamp: metricgenerator.TimeToTimestamp(fakeNow()),
			LabelValues:    []*metricspb.LabelValue{},
			Points:         []*metricspb.Point{expectedPoint},
		}
		expectedMetric := &metricspb.Metric{
			MetricDescriptor: expectedMetricDescriptor,
			Timeseries:       []*metricspb.TimeSeries{expectedTimeseries},
		}
		assert.Equal(t, expectedMetric, metrics[0])
	}
}

func checkDistributionMetricValue(t *testing.T, data consumerdata.MetricsData, name string, stats *LatencyStats) {
	found := false
	for _, metric := range data.Metrics {
		if metric.MetricDescriptor.Name == name {
			found = true
			distribution := metric.Timeseries[0].Points[0].GetDistributionValue()
			assert.Equal(t, stats.RequestCount, distribution.Count)
			assert.Equal(t, float64(stats.LatencySum), distribution.Sum)
			avg := float64(stats.LatencySum) / float64(stats.RequestCount)
			expectedDeviation := float64(stats.SumSquares) - float64(stats.RequestCount)*avg*avg
			assert.InDelta(t, expectedDeviation, distribution.SumOfSquaredDeviation, 0.000001)
			if assert.Equal(t, len(stats.Distribution), len(distribution.Buckets)) {
				for i, bucket := range distribution.Buckets {
					assert.Equal(t, stats.Distribution[i], bucket.Count)
				}
			}

		}
	}
	if !found {
		t.Errorf("Unable to find metric %s", name)
	}
}

func TestScrapeAndExport(t *testing.T) {
	consumer := &fakeConsumer{}
	collector := &NginxStatsCollector{
		consumer:       consumer,
		now:            fakeNow,
		startTime:      fakeNow(),
		done:           make(chan struct{}),
		logger:         zap.NewNop(),
		exportInterval: time.Minute,
		statsURL:       "http://success",
		getStatus:      fakeHTTPGet,
	}
	collector.scrapeAndExport()
	data := pdatautil.MetricsToMetricsData(consumer.metrics)
	assert.Len(t, data, 1)

	requestLatency := &LatencyStats{
		RequestCount: 3,
		LatencySum:   8,
		SumSquares:   24,
		Distribution: []int64{0, 2, 1},
	}
	upstreamLatency := &LatencyStats{
		RequestCount: 3,
		LatencySum:   5,
		SumSquares:   9,
		Distribution: []int64{1, 2, 0},
	}
	websocketLatency := &LatencyStats{
		RequestCount: 1,
		LatencySum:   4,
		SumSquares:   16,
		Distribution: []int64{0, 0, 1},
	}
	checkDistributionMetricValue(t, data[0], "on_vm_request_latencies", requestLatency)
	checkDistributionMetricValue(t, data[0], "on_vm_upstream_latencies", upstreamLatency)
	checkDistributionMetricValue(t, data[0], "web_socket/durations", websocketLatency)
}

func TestScrapeAndExportError(t *testing.T) {
	consumer := &fakeConsumer{}
	collector := &NginxStatsCollector{
		consumer:       consumer,
		now:            fakeNow,
		startTime:      fakeNow(),
		done:           make(chan struct{}),
		logger:         zap.NewNop(),
		exportInterval: time.Minute,
		statsURL:       "http://error",
		getStatus:      fakeHTTPGet,
	}
	collector.scrapeAndExport()
	data := pdatautil.MetricsToMetricsData(consumer.metrics)

	assert.Len(t, data, 1)
	assert.Len(t, data[0].Metrics, 0)
}
