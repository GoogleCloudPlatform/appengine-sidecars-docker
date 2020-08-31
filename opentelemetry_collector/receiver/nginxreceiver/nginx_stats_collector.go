package nginxreceiver

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io/ioutil"
	"net/http"
	"net/url"
	"time"

	metricspb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"

	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/consumer/consumerdata"
	"go.opentelemetry.io/collector/consumer/pdatautil"
	"go.uber.org/zap"

	"github.com/googlecloudplatform/appengine-sidecars-docker/opentelemetry_collector/receiver/metricgenerator"
)

// NginxStatsCollector is a struct that generates metrics by polling the nginx status page at statsURL.
type NginxStatsCollector struct {
	consumer consumer.MetricsConsumer

	now       func() time.Time
	startTime time.Time
	done      chan struct{}
	logger    *zap.Logger
	getStatus func(string) (resp *http.Response, err error)

	exportInterval time.Duration
	statsURL       string
}

// LatencyStats is a struct to parse the latency stats json into.
type LatencyStats struct {
	RequestCount int64   `json:"request_count"`
	LatencySum   int64   `json:"latency_sum"`
	SumSquares   int64   `json:"sum_squares"`
	Distribution []int64 `json:"distribution"`
}

// NginxStats is a struct to parse the nginx stats json into.
type NginxStats struct {
	RequestLatency      LatencyStats `json:"request_latency"`
	UpstreamLatency     LatencyStats `json:"upstream_latency"`
	WebsocketLatency    LatencyStats `json:"websocket_latency"`
	LatencyBucketBounds []float64    `json:"latency_bucket_bounds"`
}

// NewNginxStatsCollector creates a new NginxStatsCollector that generates metrics
// based on nginx stats found by polling the url
func NewNginxStatsCollector(interval time.Duration, statsURL string, logger *zap.Logger, consumer consumer.MetricsConsumer) (*NginxStatsCollector, error) {
	if interval <= 0 {
		return nil, errors.New("ExportInterval must be greater than 0")
	}

	if _, err := url.ParseRequestURI(statsURL); err != nil {
		return nil, fmt.Errorf("StatsURL %s is not valid: %v", statsURL, err)
	}

	collector := &NginxStatsCollector{
		consumer:       consumer,
		now:            time.Now,
		done:           make(chan struct{}),
		logger:         logger,
		exportInterval: interval,
		statsURL:       statsURL,
		getStatus:      http.Get,
	}

	return collector, nil
}

// StartCollection starts a go routine that periodically polls nginx for stats and exports metrics based on them.
func (collector *NginxStatsCollector) StartCollection() {
	collector.startTime = collector.now()

	go func() {
		ticker := time.NewTicker(collector.exportInterval)
		defer ticker.Stop()

		for {
			select {
			case <-ticker.C:
				collector.scrapeAndExport()
			case <-collector.done:
				return
			}
		}
	}()
}

// StopCollection stops the polling for nginx stats and the export of the metrics.
func (collector *NginxStatsCollector) StopCollection() {
	close(collector.done)
}

// Get the stats from the nginx latency status module and parse them into the NginxStats struct.
func (collector *NginxStatsCollector) scrapeNginxStats() (*NginxStats, error) {
	resp, err := collector.getStatus(collector.statsURL)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	body, err := ioutil.ReadAll(resp.Body)

	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("Error getting nginx stats. status code: %d content: %s", resp.StatusCode, body)
	}

	if err != nil {
		return nil, err
	}

	return readStatsJSON(body)
}

// readStatsJSON parses the stats JSON and sets defaults.
func readStatsJSON(statsJSON []byte) (*NginxStats, error) {
	// Setting the default int value to -1 makes it possible to tell when a value is missing from the json
	// since the regular defualt is 0, which is a valid value for the stats.
	stats := NginxStats{
		RequestLatency: LatencyStats{
			RequestCount: -1,
			LatencySum:   -1,
			SumSquares:   -1,
		},
		UpstreamLatency: LatencyStats{
			RequestCount: -1,
			LatencySum:   -1,
			SumSquares:   -1,
		},
		WebsocketLatency: LatencyStats{
			RequestCount: -1,
			LatencySum:   -1,
			SumSquares:   -1,
		},
	}
	if err := json.Unmarshal(statsJSON, &stats); err != nil {
		return nil, err
	}
	return &stats, nil
}

func (collector *NginxStatsCollector) appendDistributionMetric(
	stats *LatencyStats,
	bucketOptions *metricspb.DistributionValue_BucketOptions,
	metrics []*metricspb.Metric,
	descriptor *metricspb.MetricDescriptor) []*metricspb.Metric {

	sumSquaredDeviation := metricgenerator.GetSumOfSquaredDeviationsFromIntDist(
		stats.LatencySum, stats.SumSquares, stats.RequestCount)
	timeseries := metricgenerator.MakeDistributionTimeSeries(
		stats.Distribution,
		float64(stats.LatencySum),
		sumSquaredDeviation,
		stats.RequestCount,
		collector.startTime,
		collector.now(),
		bucketOptions,
		[]*metricspb.LabelValue{},
	)
	return append(metrics, &metricspb.Metric{
		MetricDescriptor: descriptor,
		Timeseries:       []*metricspb.TimeSeries{timeseries},
	},
	)
}

func (stats *LatencyStats) checkConsistency(bounds []float64) error {
	if len(bounds) == 0 || len(stats.Distribution) == 0 {
		return errors.New("One of the distribution values from the stats json is unset")
	}

	if len(bounds)+1 != len(stats.Distribution) {
		return errors.New("The length of the latency distribution and distribution bucket boundaries do not match")
	}

	if stats.RequestCount < 0 {
		return errors.New("The request count is less than 0")
	}

	if stats.SumSquares < 0 {
		return errors.New("The sum of squared latencies is less than 0")
	}

	if stats.LatencySum < 0 {
		return errors.New("The sum of latencies is less than 0")
	}

	for _, count := range stats.Distribution {
		if count < 0 {
			return errors.New("One of the latency distribution counts is less than 0")
		}
	}
	return nil
}

func (collector *NginxStatsCollector) scrapeAndExport() {
	metrics := make([]*metricspb.Metric, 0, 3)

	stats, err := collector.scrapeNginxStats()
	if err != nil {
		collector.logger.Error("Could not read nginx stats", zap.Error(err))
	} else {
		bucketOptions := metricgenerator.FormatBucketOptions(stats.LatencyBucketBounds)

		if err = stats.RequestLatency.checkConsistency(stats.LatencyBucketBounds); err != nil {
			collector.logger.Error("Invalid value received for RequestLatency", zap.Error(err))
		} else {
			metrics = collector.appendDistributionMetric(&stats.RequestLatency, bucketOptions, metrics, requestLatencyMetric)
		}
		if err = stats.WebsocketLatency.checkConsistency(stats.LatencyBucketBounds); err != nil {
			collector.logger.Error("Invalid value received for WebsocketLatency", zap.Error(err))
		} else {
			metrics = collector.appendDistributionMetric(&stats.WebsocketLatency, bucketOptions, metrics, websocketLatencyMetric)
		}

		if err = stats.UpstreamLatency.checkConsistency(stats.LatencyBucketBounds); err != nil {
			collector.logger.Error("Invalid value received for UpstreamLatency", zap.Error(err))
		} else {
			metrics = collector.appendDistributionMetric(&stats.UpstreamLatency, bucketOptions, metrics, upstreamLatencyMetric)
		}
	}

	ctx := context.Background()
	md := consumerdata.MetricsData{Metrics: metrics}
	collector.consumer.ConsumeMetrics(ctx, pdatautil.MetricsFromMetricsData([]consumerdata.MetricsData{md}))
}
