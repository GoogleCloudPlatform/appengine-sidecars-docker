package vmimageagereceiver

import (
	"context"
	"errors"
	"time"

	metricspb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"
	"github.com/open-telemetry/opentelemetry-collector/consumer"
	"github.com/open-telemetry/opentelemetry-collector/consumer/consumerdata"

	"github.com/googlecloudplatform/appengine-sidecars-docker/opentelemetry_collector/receiver/metricgenerator"
)

// VMImageAgeCollector is a struct that generates metrics based on the
// VM image age in the config.
type VMImageAgeCollector struct {
	consumer consumer.MetricsConsumer

	startTime time.Time

	exportInterval time.Duration
	buildDate      string
	done           chan struct{}
	vmImageName    string

	parsedBuildDate time.Time
	buildDateError  bool
	bucketOptions   *metricspb.DistributionValue_BucketOptions
	labelValues     []*metricspb.LabelValue
}

const (
	defaultExportInterval = 10 * time.Minute
)

// NewVMImageAgeCollector creates a new VMImageAgeCollector that generates metrics
// based on the buildDate and vmImageName.
func NewVMImageAgeCollector(exportInterval time.Duration, buildDate, vmImageName string, consumer consumer.MetricsConsumer) *VMImageAgeCollector {
	if exportInterval <= 0 {
		exportInterval = defaultExportInterval
	}

	collector := &VMImageAgeCollector{
		consumer:       consumer,
		startTime:      time.Now(),
		buildDate:      buildDate,
		vmImageName:    vmImageName,
		exportInterval: exportInterval,
		done:           make(chan struct{}),
	}

	return collector
}

func (collector *VMImageAgeCollector) parseBuildDate() {
	var err error
	collector.parsedBuildDate, err = time.Parse(time.RFC3339, collector.buildDate)
	collector.buildDateError = err != nil
}

func calculateImageAge(buildDate time.Time, now time.Time) (float64, error) {
	imageAge := now.Sub(buildDate)
	imageAgeDays := imageAge.Hours() / 24
	if imageAgeDays < 0 {
		return 0, errors.New("The vm build date is more recent than the current time")
	}
	return imageAgeDays, nil
}

// StartCollection starts a go routine with a ticker that periodically generates and exports the metrics.
func (collector *VMImageAgeCollector) StartCollection() {
	collector.setupCollection()

	go func() {
		ticker := time.NewTicker(collector.exportInterval)
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

func (collector *VMImageAgeCollector) setupCollection() {
	collector.parseBuildDate()
	collector.bucketOptions = metricgenerator.MakeExponentialBucketOptions(boundsBase, numBounds)
	collector.labelValues = []*metricspb.LabelValue{metricgenerator.MakeLabelValue(collector.vmImageName)}
}

// StopCollection stops the generation and export of the metrics.
func (collector *VMImageAgeCollector) StopCollection() {
	close(collector.done)
}

func (collector *VMImageAgeCollector) makeErrorMetrics() *metricspb.Metric {
	timeseries := metricgenerator.MakeInt64TimeSeries(1, collector.startTime, time.Now(), collector.labelValues)
	return &metricspb.Metric{
		MetricDescriptor: vmImageErrorMetric,
		Timeseries:       []*metricspb.TimeSeries{timeseries},
	}

}

func (collector *VMImageAgeCollector) scrapeAndExport() {
	metrics := make([]*metricspb.Metric, 0, 1)

	if collector.buildDateError {
		metrics = append(metrics, collector.makeErrorMetrics())
	} else {
		imageAge, err := calculateImageAge(collector.parsedBuildDate, time.Now())
		if err != nil {
			metrics = append(metrics, collector.makeErrorMetrics())
		} else {
			timeseries := metricgenerator.MakeSingleValueDistributionTimeSeries(
				imageAge, collector.startTime, time.Now(), collector.bucketOptions, collector.labelValues)
			metrics = append(
				metrics,
				&metricspb.Metric{
					MetricDescriptor: vmImageAgeMetric,
					Timeseries:       []*metricspb.TimeSeries{timeseries},
				},
			)
		}
	}

	ctx := context.Background()
	collector.consumer.ConsumeMetricsData(ctx, consumerdata.MetricsData{Metrics: metrics})
}
