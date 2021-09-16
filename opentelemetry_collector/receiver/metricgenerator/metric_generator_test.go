package metricgenerator

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	timestamp "google.golang.org/protobuf/types/known/timestamppb"

	metricspb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"
)

func Test_MakeInt64TimeSeries(t *testing.T) {
	seconds1 := int64(1541015015)
	seconds2 := int64(1541015016)
	nanoseconds := int64(123456789)
	startTime := time.Unix(seconds1, nanoseconds)
	currentTime := time.Unix(seconds2, nanoseconds)
	labelValues := []*metricspb.LabelValue{MakeLabelValue("test_label")}
	timeseries := MakeInt64TimeSeries(1, startTime, currentTime, labelValues)

	expectedTimeseries := &metricspb.TimeSeries{
		StartTimestamp: timestamp.New(startTime),
		LabelValues:    labelValues,
		Points: []*metricspb.Point{{
			Timestamp: timestamp.New(currentTime),
			Value: &metricspb.Point_Int64Value{
				Int64Value: 1,
			},
		}},
	}
	assert.Equal(t, timeseries, expectedTimeseries)
}

func Test_MakeDoubleTimeSeries(t *testing.T) {
	seconds1 := int64(1541015015)
	seconds2 := int64(1541015016)
	nanoseconds := int64(123456789)
	startTime := time.Unix(seconds1, nanoseconds)
	currentTime := time.Unix(seconds2, nanoseconds)
	labelValues := []*metricspb.LabelValue{MakeLabelValue("test_label")}
	timeseries := MakeDoubleTimeSeries(1.1, startTime, currentTime, labelValues)

	expectedTimeseries := &metricspb.TimeSeries{
		StartTimestamp: timestamp.New(startTime),
		LabelValues:    labelValues,
		Points: []*metricspb.Point{{
			Timestamp: timestamp.New(currentTime),
			Value: &metricspb.Point_DoubleValue{
				DoubleValue: 1.1,
			},
		}},
	}
	assert.Equal(t, timeseries, expectedTimeseries)
}

func Test_MakeExponentialBucketOptions(t *testing.T) {
	bucketOptions := MakeExponentialBucketOptions(2, 5)
	expectedBounds := []float64{1, 2, 4, 8, 16, 32}

	assert.Equal(t, bucketOptions.GetExplicit().Bounds, expectedBounds)
}

func Test_MakeBuckets(t *testing.T) {
	bounds := []float64{1, 2, 4}
	values := []float64{1.5, 3}
	buckets := MakeBuckets(values, bounds)

	expectedBuckets := []*metricspb.DistributionValue_Bucket{
		{Count: 0},
		{Count: 1},
		{Count: 1},
		{Count: 0},
	}
	assert.Equal(t, buckets, expectedBuckets)
}

func Test_MakeBucketsMultipleValueInOneBucket(t *testing.T) {
	bounds := []float64{1, 2, 4}
	values := []float64{3, 3}
	buckets := MakeBuckets(values, bounds)

	expectedBuckets := []*metricspb.DistributionValue_Bucket{
		{Count: 0},
		{Count: 0},
		{Count: 2},
		{Count: 0},
	}
	assert.Equal(t, buckets, expectedBuckets)
}

func Test_MakeBucketsWith0Value(t *testing.T) {
	bounds := []float64{1, 2, 4}
	values := []float64{0, 3}
	buckets := MakeBuckets(values, bounds)

	expectedBuckets := []*metricspb.DistributionValue_Bucket{
		{Count: 1},
		{Count: 0},
		{Count: 1},
		{Count: 0},
	}
	assert.Equal(t, buckets, expectedBuckets)
}

func Test_MakeBucketsWithValueOverHighestBound(t *testing.T) {
	bounds := []float64{1, 2, 4}
	values := []float64{10, 3}
	buckets := MakeBuckets(values, bounds)

	expectedBuckets := []*metricspb.DistributionValue_Bucket{
		{Count: 0},
		{Count: 0},
		{Count: 1},
		{Count: 1},
	}
	assert.Equal(t, buckets, expectedBuckets)
}

func Test_GetBucketIndex(t *testing.T) {
	bounds := []float64{1, 2, 4}

	assert.Equal(t, getBucketIndex(3, bounds), 2)
}

func Test_GetBucketIndexWith0(t *testing.T) {
	bounds := []float64{1, 2, 4}
	assert.Equal(t, getBucketIndex(0, bounds), 0)
}

func Test_GetBucketIndexWithValueOverHighestBound(t *testing.T) {
	bounds := []float64{1, 2, 4}
	assert.Equal(t, getBucketIndex(10, bounds), 3)
}

func Test_MakeSingleDistributionTimeSeries(t *testing.T) {
	seconds1 := int64(1541015015)
	seconds2 := int64(1541015016)
	nanoseconds := int64(123456789)
	startTime := time.Unix(seconds1, nanoseconds)
	currentTime := time.Unix(seconds2, nanoseconds)

	bucketOptions := MakeExponentialBucketOptions(2, 2)
	labelValues := []*metricspb.LabelValue{MakeLabelValue("test_label")}
	timeseries := MakeSingleValueDistributionTimeSeries(3, startTime, currentTime, bucketOptions, labelValues)

	assert.Equal(t, timeseries.StartTimestamp, timestamp.New(startTime))
	assert.Equal(t, timeseries.LabelValues, labelValues)
	assert.Len(t, timeseries.Points, 1)
	assert.Equal(t, timeseries.Points[0].Timestamp, timestamp.New(currentTime))

	distribution := timeseries.Points[0].GetDistributionValue()
	assert.Equal(t, distribution.GetCount(), int64(1))
	assert.Equal(t, distribution.GetSum(), float64(3))
	assert.Equal(t, distribution.GetSumOfSquaredDeviation(), float64(0))
	assert.Equal(t, distribution.GetBucketOptions(), bucketOptions)

	expectedBuckets := []*metricspb.DistributionValue_Bucket{
		{Count: 0},
		{Count: 0},
		{Count: 1},
		{Count: 0},
	}
	assert.Equal(t, distribution.GetBuckets(), expectedBuckets)
}

func Test_FormatBucketOptions(t *testing.T) {
	bucketOptions := FormatBucketOptions([]float64{2, 4, 10})
	expectedBucketOptions := &metricspb.DistributionValue_BucketOptions{
		Type: &metricspb.DistributionValue_BucketOptions_Explicit_{
			Explicit: &metricspb.DistributionValue_BucketOptions_Explicit{
				Bounds: []float64{2, 4, 10},
			},
		},
	}
	assert.Equal(t, expectedBucketOptions, bucketOptions)
}

func Test_FormatBuckets(t *testing.T) {
	buckets := formatBuckets([]int64{1, 4, 3})
	expectedBuckets := []*metricspb.DistributionValue_Bucket{
		{Count: 1},
		{Count: 4},
		{Count: 3},
	}
	assert.Equal(t, expectedBuckets, buckets)
}

func Test_GetSumOfSquaredDeviationsFromInt(t *testing.T) {
	deviation := GetSumOfSquaredDeviationsFromIntDist(9, 33, 3)
	assert.Equal(t, float64(6), deviation)
}

func Test_GetSumOfSquaredDeviationsFromIntWithZero(t *testing.T) {
	deviation := GetSumOfSquaredDeviationsFromIntDist(0, 0, 0)
	assert.Equal(t, float64(0), deviation)
}

func Test_GetSumOfSquaredDeviationsFromIntFraction(t *testing.T) {
	deviation := GetSumOfSquaredDeviationsFromIntDist(5, 13, 2)
	assert.Equal(t, 0.5, deviation)
}
