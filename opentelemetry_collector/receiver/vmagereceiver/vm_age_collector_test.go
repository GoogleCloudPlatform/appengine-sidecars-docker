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
	testVMStartTime      = "2007-01-01T10:01:00.123456789+00:00"
	testVMReadyTime      = "2007-01-01T10:02:00.123456789+00:00"
	testVMImageBuildDate = "2006-01-02T15:04:05.123456789+00:00"
	testVMImageName      = "test_image_name"
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
	collector := NewVMAgeCollector(0, testVMImageBuildDate, testVMImageName, testVMStartTime, testVMReadyTime, nil, zap.NewNop())
	collector.setupCollection()

	assert.False(t, collector.buildDateError)
	assert.False(t, collector.vmStartTimeError)
	assert.False(t, collector.vmReadyTimeError)

	diff := collector.parsedBuildDate.Sub(time.Date(2006, time.January, 2, 15, 4, 5, 123456789, time.FixedZone("", 0)))
	assert.Equal(t, diff, time.Second*0)
}

func TestParseInputTimesError(t *testing.T) {
	type test struct {
		buildDate        string
		vmStartTime      string
		vmReadyTime      string
		buildDateError   bool
		vmStartTimeError bool
		vmReadyTimeError bool
	}

	tests := []test{
		{
			buildDate:        "misformated_date",
			vmStartTime:      testVMStartTime,
			vmReadyTime:      testVMReadyTime,
			buildDateError:   true,
			vmStartTimeError: false,
			vmReadyTimeError: false,
		},
		{
			buildDate:        testVMImageBuildDate,
			vmStartTime:      "misformated_date",
			vmReadyTime:      testVMReadyTime,
			buildDateError:   false,
			vmStartTimeError: true,
			vmReadyTimeError: false,
		},
		{
			buildDate:        testVMImageBuildDate,
			vmStartTime:      testVMStartTime,
			vmReadyTime:      "misformated_date",
			buildDateError:   false,
			vmStartTimeError: false,
			vmReadyTimeError: true,
		},
	}

	for _, tc := range tests {
		collector := NewVMAgeCollector(0, tc.buildDate, testVMImageName, tc.vmStartTime, tc.vmReadyTime, nil, zap.NewNop())
		collector.setupCollection()
		assert.Equal(t, tc.buildDateError, collector.buildDateError)
		assert.Equal(t, tc.vmStartTimeError, collector.vmStartTimeError)
		assert.Equal(t, tc.vmReadyTime, collector.vmReadyTime)
	}
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

func TestScrapeAndExportVMImageAge(t *testing.T) {
	consumer := fakeConsumer{storage: &metricsStore{}}
	collector := NewVMAgeCollector(0, testVMImageBuildDate, testVMImageName, testVMStartTime, testVMReadyTime, consumer, zap.NewNop())
	collector.setupCollection()

	expectedDesc := &metricspb.MetricDescriptor{
		Name:        "vm_image_age",
		Description: "The VM image age for the VM instance",
		Unit:        "Days",
		Type:        metricspb.MetricDescriptor_GAUGE_DOUBLE,
		LabelKeys: []*metricspb.LabelKey{{
			Key: "vm_image_name",
		}},
	}

	collector.scrapeAndExportVMImageAge()
	assertMetricGreaterThan0Double(t, expectedDesc, consumer.storage.metrics)
}

func scrapeAndExportVMReadyTime(t *testing.T) {
	consumer := fakeConsumer{storage: &metricsStore{}}
	collector := NewVMAgeCollector(0, testVMImageBuildDate, testVMImageName, testVMStartTime, testVMReadyTime, consumer, zap.NewNop())
	collector.setupCollection()

	expectedDesc := &metricspb.MetricDescriptor{
		Name:        "vm_image_age",
		Description: "The VM image age for the VM instance",
		Unit:        "Days",
		Type:        metricspb.MetricDescriptor_GAUGE_DOUBLE,
		LabelKeys: []*metricspb.LabelKey{{
			Key: "vm_image_name",
		}},
	}

	collector.scrapeAndExportVMReadyTime(float64(60))
	assertMetricGreaterThan0Double(t, expectedDesc, consumer.storage.metrics)
}

func TestScrapeAndExportVMImageAgeWithError(t *testing.T) {
	consumer := fakeConsumer{storage: &metricsStore{}}
	collector := NewVMAgeCollector(0, "", testVMImageName, testVMStartTime, testVMReadyTime, consumer, zap.NewNop())
	collector.setupCollection()

	expectedMetricDescriptor := &metricspb.MetricDescriptor{
		Name:        "vm_image_ages_error",
		Description: "The current number of VM instances with errors exporting the VM image age.",
		Unit:        "Count",
		Type:        metricspb.MetricDescriptor_GAUGE_INT64,
		LabelKeys: []*metricspb.LabelKey{{
			Key: "vm_image_name",
		}},
	}

	collector.scrapeAndExportVMImageAge()

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

func assertMetricGreaterThan0Double(t *testing.T, expectedMetricDescriptor *metricspb.MetricDescriptor, metrics pdata.Metrics) {
	// TODO: Rewrite tests to directly use pdata.Metrics instead of converting back to consumerdata.MetricsData.
	cdMetrics := internaldata.MetricsToOC(metrics)[0]
	if assert.Len(t, cdMetrics.Metrics, 1) {
		actualMetric := cdMetrics.Metrics[0]
		assert.Equal(t, expectedMetricDescriptor, actualMetric.MetricDescriptor)

		if assert.Len(t, actualMetric.Timeseries, 1) {
			expectedLabel := []*metricspb.LabelValue{{Value: testVMImageName, HasValue: true}}
			timeseries := actualMetric.Timeseries[0]
			assert.Equal(t, expectedLabel, timeseries.LabelValues)

			if assert.Len(t, timeseries.Points, 1) {
				assert.Greater(t, timeseries.Points[0].GetDoubleValue(), 0.0)
			}
		}
	}
}
