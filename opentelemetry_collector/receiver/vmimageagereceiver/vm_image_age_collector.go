package vmimageagereceiver

import (
	"context"
	"errors"
	"time"

	metricspb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"

	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/consumer/consumerdata"
	"go.opentelemetry.io/collector/translator/internaldata"	
	"go.uber.org/zap"

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
	logger         *zap.Logger

	parsedBuildDate time.Time
	buildDateError  bool
	labelValues     []*metricspb.LabelValue
}

const (
	defaultExportInterval = 10 * time.Minute
)

// NewVMImageAgeCollector creates a new VMImageAgeCollector that generates metrics
// based on the buildDate and vmImageName.
func NewVMImageAgeCollector(exportInterval time.Duration, buildDate, vmImageName string, consumer consumer.MetricsConsumer, logger *zap.Logger) *VMImageAgeCollector {
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
		logger:         logger,
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
			timeseries := metricgenerator.MakeDoubleTimeSeries(
				imageAge, collector.startTime, time.Now(), collector.labelValues)
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
	md := consumerdata.MetricsData{Metrics: metrics}
	err := collector.consumer.ConsumeMetrics(ctx, internaldata.OCSliceToMetrics([]consumerdata.MetricsData{md}))
	if err != nil {
		collector.logger.Error("Error sending metrics", zap.Error(err))
	}
}
