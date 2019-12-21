package vmimageagereceiver

import (
	"context"
	"errors"
	"fmt"
	"time"

	"contrib.go.opencensus.io/resource/auto"
	metricspb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"
	resourcepb "github.com/census-instrumentation/opencensus-proto/gen-go/resource/v1"
	"github.com/open-telemetry/opentelemetry-collector/consumer"
	"github.com/open-telemetry/opentelemetry-collector/consumer/consumerdata"

	"github.com/googlecloudplatform/appengine-sidecars-docker/opencensus-monitoring/receiver/metricgenerator"
)

type VmImageAgeCollector struct {
	consumer consumer.MetricsConsumer

	startTime time.Time

	exportInterval time.Duration
	buildDate      string
	done           chan struct{}

	parsedBuildDate time.Time
	buildDateError  bool
	bucketOptions   *metricspb.DistributionValue_BucketOptions
}

const (
	defaultExportInterval = 1 * time.Minute
)

var rsc *resourcepb.Resource

func NewVmImageAgeCollector(exportInterval time.Duration, buildDate string, consumer consumer.MetricsConsumer) *VmImageAgeCollector {
	if exportInterval <= 0 {
		exportInterval = defaultExportInterval
	}

	collector := &VmImageAgeCollector{
		consumer:       consumer,
		startTime:      time.Now(),
		buildDate:      buildDate,
		exportInterval: exportInterval,
		done:           make(chan struct{}),
	}

	return collector
}

func detectResource() {
	autoRes, err := auto.Detect(context.Background())
	if err != nil {
		panic(fmt.Sprintf("Resource detection failed, err: %v", err))
	}
	if autoRes != nil {
		rsc = &resourcepb.Resource{
			Type:   autoRes.Type,
			Labels: autoRes.Labels,
		}
	}
}

func (collector *VmImageAgeCollector) parseBuildDate() {
	var err error
	collector.parsedBuildDate, err = time.Parse(time.RFC3339, collector.buildDate)
	collector.buildDateError = err != nil
}

func calculateImageAge(buildDate time.Time) (float64, error) {
	imageAge := time.Now().Sub(buildDate)
	imageAgeDays := imageAge.Hours() / 24
	if imageAgeDays < 0 {
		return 0, errors.New("The vm build date is more recent than the current time")
	}
	return imageAgeDays, nil
}

func (collector *VmImageAgeCollector) StartCollection() {
	detectResource()
	collector.parseBuildDate()
	collector.bucketOptions = metricgenerator.MakeExponentialBucketOptions(boundsBase, numBounds)

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

func (collector *VmImageAgeCollector) StopCollection() {
	close(collector.done)
}

func (collector *VmImageAgeCollector) makeErrorMetrics() *metricspb.Metric {
	return &metricspb.Metric{
                                MetricDescriptor: vmImageErrorMetric,
                                Resource:         rsc,
                                Timeseries:       []*metricspb.TimeSeries{metricgenerator.MakeInt64TimeSeries(1, collector.startTime)},
                        }

}

func (collector *VmImageAgeCollector) scrapeAndExport() {
	metrics := make([]*metricspb.Metric, 0, 1)

	if collector.buildDateError{
		metrics = append(metrics, collector.makeErrorMetrics())
	} else {
		imageAge, err := calculateImageAge(collector.parsedBuildDate)
		if err != nil {
			metrics = append(metrics, collector.makeErrorMetrics())
		} else{
		metrics = append(
			metrics,
			&metricspb.Metric{
				MetricDescriptor: vmImageAgeMetric,
				Resource:         rsc,
				Timeseries:       []*metricspb.TimeSeries{metricgenerator.MakeSingleValueDistributionTimeSeries(imageAge, collector.startTime, collector.bucketOptions)},
			},
		)
	}
	}

	ctx := context.Background()
	collector.consumer.ConsumeMetricsData(ctx, consumerdata.MetricsData{Metrics: metrics})
}
