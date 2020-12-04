package vmagereceiver

import (
	"context"
	"testing"
	"time"

	metricspb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"
	"github.com/stretchr/testify/assert"

	"go.opentelemetry.io/collector/consumer/pdata"
	"go.opentelemetry.io/collector/translator/internaldata"
	"go.uber.org/zap"
)

const (
	testVMStartTime = "2007-01-01T10:01:00+00:00"
	testVMReadyTime = "2007-01-01T10:02:00+00:00"
	testVMImageName = "test_image_name"
)

func TestCalculateImageAge(t *testing.T) {
	now := time.Date(2020, time.January, 29, 0, 0, 0, 0, time.UTC)
	buildTime := time.Date(2019, time.December, 31, 0, 0, 0, 0, time.UTC)

	age, err := calculateImageAge(buildTime, now)
	assert.Nil(t, err)
	assert.Equal(t, float64(29), age)
}

func TestCalculateImageAgeWithNegativeAge(t *testing.T) {
	now := time.Date(2020, time.January, 29, 12, 30, 0, 0, time.UTC)
	buildTime := time.Date(2020, time.January, 31, 12, 30, 0, 0, time.UTC)

	_, err := calculateImageAge(buildTime, now)
	assert.Error(t, err)
}

func TestCalculateImageAgeWith0Age(t *testing.T) {
	now := time.Date(2020, time.January, 29, 12, 0, 0, 0, time.UTC)
	buildTime := time.Date(2020, time.January, 29, 6, 0, 0, 0, time.UTC)

	age, err := calculateImageAge(buildTime, now)
	assert.Nil(t, err)
	assert.Equal(t, float64(0.25), age)
}

func TestParseInputTimes(t *testing.T) {
	collector := NewVMAgeCollector(0, "2006-01-02T15:04:05+00:00", testVMImageName, testVMStartTime, testVMReadyTime, nil, nil)
	collector.setupCollection()

	assert.False(t, collector.buildDateError)
	assert.False(t, collector.vmStartTimeError)
	assert.False(t, collector.vmReadyTimeError)

	diff := collector.parsedBuildDate.Sub(time.Date(2006, time.January, 2, 15, 4, 5, 0, time.FixedZone("", 0)))
	assert.Equal(t, diff, time.Second*0)
}

func TestParseBuildDateError(t *testing.T) {
	collector := NewVMAgeCollector(0, "misformated_date", testVMImageName, testVMStartTime, testVMReadyTime, nil, nil)
	collector.setupCollection()
	assert.True(t, collector.buildDateError)
}

type fakeConsumer struct {
	storage *metricsStore
}

type metricsStore struct {
	metrics pdata.Metrics
}

func (s *metricsStore) storeMetric(toStore pdata.Metrics) {
	s.metrics = toStore
}

func (consumer fakeConsumer) ConsumeMetrics(ctx context.Context, metrics pdata.Metrics) error {
	consumer.storage.storeMetric(metrics)
	return nil
}

func TestScrapeAndExport(t *testing.T) {
	type test struct {
		exportFunc   func()
		expectedDesc *metricspb.MetricDescriptor
		metricType   string
	}

	consumer := fakeConsumer{storage: &metricsStore{}}
	collector := NewVMAgeCollector(0, "2006-01-02T15:04:05+00:00", testVMImageName, testVMStartTime, testVMReadyTime, consumer, zap.NewNop())
	collector.setupCollection()

	tests := []test{
		{
			exportFunc: collector.scrapeAndExportVMImageAge,
			expectedDesc: &metricspb.MetricDescriptor{
				Name:        "vm_image_age",
				Description: "The VM image age for the VM instance",
				Unit:        "Days",
				Type:        metricspb.MetricDescriptor_GAUGE_DOUBLE,
				LabelKeys: []*metricspb.LabelKey{{
					Key: "vm_image_name",
				}},
			},
			metricType: "double",
		},
		{
			exportFunc: collector.scrapeAndExportVMStartTime,
			expectedDesc: &metricspb.MetricDescriptor{
				Name:        "vm_start_time",
				Description: "The time that the VM startup script began to run.",
				Unit:        "Seconds",
				Type:        metricspb.MetricDescriptor_GAUGE_INT64,
				LabelKeys: []*metricspb.LabelKey{{
					Key: "vm_image_name",
				}},
			},
			metricType: "int64",
		},
		{
			exportFunc: collector.scrapeAndExportVMReadyTime,
			expectedDesc: &metricspb.MetricDescriptor{
				Name:        "vm_ready_time",
				Description: "The amount of time from when Flex first started setting up the VM in the startup script to when it finished setting up all VM runtime components.",
				Unit:        "Seconds",
				Type:        metricspb.MetricDescriptor_GAUGE_INT64,
				LabelKeys: []*metricspb.LabelKey{{
					Key: "vm_image_name",
				}},
			},
			metricType: "int64",
		},
	}

	for _, tc := range tests {
		tc.exportFunc()

		// TODO: Rewrite tests to directly use pdata.Metrics instead of converting back to consumerdata.MetricsData.
		cdMetrics := internaldata.MetricsToOC(consumer.storage.metrics)[0]
		if assert.Len(t, cdMetrics.Metrics, 1) {
			actualMetric := cdMetrics.Metrics[0]
			assert.Equal(t, tc.expectedDesc, actualMetric.MetricDescriptor)

			if assert.Len(t, actualMetric.Timeseries, 1) {
				expectedLabel := []*metricspb.LabelValue{{Value: testVMImageName, HasValue: true}}
				timeseries := actualMetric.Timeseries[0]
				assert.Equal(t, expectedLabel, timeseries.LabelValues)

				if assert.Len(t, timeseries.Points, 1) {
					switch tc.metricType {
					case "double":
						assert.Greater(t, timeseries.Points[0].GetDoubleValue(), 0.0)
					case "int64":
						assert.Greater(t, timeseries.Points[0].GetInt64Value(), int64(0))
					}
				}
			}
		}
	}
}

func TestScrapeAndExportVMImageAgeWithError(t *testing.T) {
	consumer := fakeConsumer{storage: &metricsStore{}}
	collector := NewVMAgeCollector(0, "", testVMImageName, testVMStartTime, testVMReadyTime, consumer, zap.NewNop())
	collector.setupCollection()
	collector.scrapeAndExportVMImageAge()

	expectedMetricDescriptor := &metricspb.MetricDescriptor{
		Name:        "vm_image_ages_error",
		Description: "The current number of VM instances with errors exporting the VM image age.",
		Unit:        "Count",
		Type:        metricspb.MetricDescriptor_GAUGE_INT64,
		LabelKeys: []*metricspb.LabelKey{{
			Key: "vm_image_name",
		}},
	}

	// TODO: Rewrite tests to directly use pdata.Metrics instead of converting back to consumerdata.MetricsData.
	cdMetrics := internaldata.MetricsToOC(consumer.storage.metrics)[0]
	if assert.Len(t, cdMetrics.Metrics, 1) {

		actualMetric := cdMetrics.Metrics[0]
		assert.Equal(t, expectedMetricDescriptor, actualMetric.MetricDescriptor)

		if assert.Len(t, actualMetric.Timeseries, 1) {
			expectedLabel := []*metricspb.LabelValue{{Value: testVMImageName, HasValue: true}}
			timeseries := actualMetric.Timeseries[0]
			assert.Equal(t, expectedLabel, timeseries.LabelValues)

			if assert.Len(t, timeseries.Points, 1) {
				assert.Equal(t, int64(1), timeseries.Points[0].GetInt64Value())
			}
		}
	}
}
