package vmagereceiver

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

// VMAgeCollector is a struct that generates metrics based on the
// VM image age in the config.
type VMAgeCollector struct {
	consumer consumer.MetricsConsumer

	collectorStartTime time.Time

	exportInterval time.Duration
	buildDate      string
	done           chan struct{}
	vmImageName    string
	vmStartTime    string
	vmReadyTime    string

	logger *zap.Logger

	parsedBuildDate   time.Time
	buildDateError    bool
	parsedVMStartTime time.Time
	vmStartTimeError  bool
	parsedVMReadyTime time.Time
	vmReadyTimeError  bool

	labelValues []*metricspb.LabelValue
}

const (
	defaultExportInterval = 10 * time.Minute
)

// NewVMAgeCollector creates a new VMAgeCollector that generates metrics
// based on the buildDate and vmImageName.
func NewVMAgeCollector(exportInterval time.Duration, buildDate, vmImageName, vmStartTime, vmReadyTime string, consumer consumer.MetricsConsumer, logger *zap.Logger) *VMAgeCollector {
	if exportInterval <= 0 {
		exportInterval = defaultExportInterval
	}

	collector := &VMAgeCollector{
		consumer:           consumer,
		collectorStartTime: time.Now(),
		buildDate:          buildDate,
		vmImageName:        vmImageName,
		vmStartTime:        vmStartTime,
		vmReadyTime:        vmReadyTime,
		exportInterval:     exportInterval,
		done:               make(chan struct{}),
		logger:             logger,
	}

	return collector
}

func (collector *VMAgeCollector) parseDate(date string) (time.Time, error) {
	return time.Parse(time.RFC3339Nano, date)
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
func (collector *VMAgeCollector) StartCollection() {
	collector.setupCollection()
	readyTime := float64(collector.parsedVMReadyTime.Sub(collector.parsedVMStartTime) / time.Second)

	go func() {
		ticker := time.NewTicker(collector.exportInterval)
		for {
			select {
			case <-ticker.C:
				collector.scrapeAndExportVMImageAge()
				collector.scrapeAndExportVMReadyTime(readyTime)
			case <-collector.done:
				return
			}
		}
	}()
}

func (collector *VMAgeCollector) setupCollection() {
	var err error

	collector.parsedBuildDate, err = collector.parseDate(collector.buildDate)
	collector.buildDateError = (err != nil)

	collector.parsedVMStartTime, err = collector.parseDate(collector.vmStartTime)
	if err != nil {
		collector.vmStartTimeError = true
		collector.logger.Error("Error parsing vmStartTime", zap.Error(err))
	}

	collector.parsedVMReadyTime, err = collector.parseDate(collector.vmReadyTime)
	collector.vmReadyTimeError = (err != nil)
	if err != nil {
		collector.vmReadyTimeError = true
		collector.logger.Error("Error parsing vmReadyTime", zap.Error(err))
	}

	collector.labelValues = []*metricspb.LabelValue{metricgenerator.MakeLabelValue(collector.vmImageName)}
}

// StopCollection stops the generation and export of the metrics.
func (collector *VMAgeCollector) StopCollection() {
	close(collector.done)
}

func (collector *VMAgeCollector) makeErrorMetrics() *metricspb.Metric {
	timeseries := metricgenerator.MakeInt64TimeSeries(1, collector.collectorStartTime, time.Now(), collector.labelValues)
	return &metricspb.Metric{
		MetricDescriptor: vmImageAgesErrorMetric,
		Timeseries:       []*metricspb.TimeSeries{timeseries},
	}
}

func makeMetrics(metricDescriptor *metricspb.MetricDescriptor, timeseries *metricspb.TimeSeries) []*metricspb.Metric {
	return []*metricspb.Metric{
		{
			MetricDescriptor: metricDescriptor,
			Timeseries:       []*metricspb.TimeSeries{timeseries},
		},
	}
}

func (collector *VMAgeCollector) export(metrics []*metricspb.Metric, errorKey string) {
	ctx := context.Background()
	md := consumerdata.MetricsData{Metrics: metrics}
	err := collector.consumer.ConsumeMetrics(ctx, internaldata.OCSliceToMetrics([]consumerdata.MetricsData{md}))
	if err != nil {
		collector.logger.Error(errorKey, zap.Error(err))
	}
}

func (collector *VMAgeCollector) scrapeAndExportVMImageAge() {
	var metrics []*metricspb.Metric

	if collector.buildDateError {
		metrics = []*metricspb.Metric{collector.makeErrorMetrics()}
	} else {
		imageAge, err := calculateImageAge(collector.parsedBuildDate, time.Now())
		if err != nil {
			metrics = []*metricspb.Metric{collector.makeErrorMetrics()}
		} else {
			timeseries := metricgenerator.MakeDoubleTimeSeries(imageAge, collector.collectorStartTime, time.Now(), collector.labelValues)
			metrics = makeMetrics(vmImageAgeMetric, timeseries)
		}
	}

	collector.export(metrics, "Error sending VM image age metrics")
}

func (collector *VMAgeCollector) scrapeAndExportVMReadyTime(readyTime float64) {
	if collector.vmReadyTimeError {
		return
	}

	timeseries := metricgenerator.MakeDoubleTimeSeries(readyTime, collector.collectorStartTime, time.Now(), collector.labelValues)
	collector.export(makeMetrics(vmReadyTimeMetric, timeseries), "Error sending VM ready time metrics")
}
