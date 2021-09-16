package metricgenerator

import (
	"math"
	"time"

	timestamp "google.golang.org/protobuf/types/known/timestamppb"

	metricspb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"
)

// MakeInt64TimeSeries generates a proto representation of a timeseries containing a single point for an int64 metric.
func MakeInt64TimeSeries(val int64, startTime, now time.Time, labels []*metricspb.LabelValue) *metricspb.TimeSeries {
	return &metricspb.TimeSeries{
		StartTimestamp: timestamp.New(startTime),
		LabelValues:    labels,
		Points:         []*metricspb.Point{{Timestamp: timestamp.New(now), Value: &metricspb.Point_Int64Value{Int64Value: val}}},
	}
}

// MakeDoubleTimeSeries generates a proto representation of a timeseries containing a single point for an double metric.
func MakeDoubleTimeSeries(val float64, startTime, now time.Time, labels []*metricspb.LabelValue) *metricspb.TimeSeries {
	return &metricspb.TimeSeries{
		StartTimestamp: timestamp.New(startTime),
		LabelValues:    labels,
		Points:         []*metricspb.Point{{Timestamp: timestamp.New(now), Value: &metricspb.Point_DoubleValue{DoubleValue: val}}},
	}
}

// MakeExponentialBucketOptions generates a proto representation of a config which,
// defines a distribution's bounds. This defines maxExponent + 2 buckets. The boundaries for bucket
// index i are:
//
// [0, boundsBase ^ i) for i == 0
// [boundsBase ^ (i - 1)], boundsBase ^ i) for 0 < i <= maxExponent
// [boundsBase ^ (i - 1), +infinity) for i == maxExponent + 1
func MakeExponentialBucketOptions(boundsBase, maxExponent float64) *metricspb.DistributionValue_BucketOptions {
	bounds := make([]float64, 0, int(maxExponent))
	for i := float64(0); i <= maxExponent; i++ {
		bounds = append(bounds, math.Pow(boundsBase, i))
	}
	return &metricspb.DistributionValue_BucketOptions{
		Type: &metricspb.DistributionValue_BucketOptions_Explicit_{
			Explicit: &metricspb.DistributionValue_BucketOptions_Explicit{
				Bounds: bounds,
			},
		},
	}
}

// FormatBucketOptions formats a []float64 bounds as a DistributionValue_BucketOptions proto.
// bounds should list the upper boundaries of the distribution buckets,
// except for the last bucket which has +infinity as an implied upper bound.
func FormatBucketOptions(bounds []float64) *metricspb.DistributionValue_BucketOptions {
	return &metricspb.DistributionValue_BucketOptions{
		Type: &metricspb.DistributionValue_BucketOptions_Explicit_{
			Explicit: &metricspb.DistributionValue_BucketOptions_Explicit{
				Bounds: bounds,
			},
		},
	}
}

// MakeBuckets generates a proto representation of a distribution containing a single value.
// bounds defines the bucket boundaries of the distribution.
func MakeBuckets(values, bounds []float64) []*metricspb.DistributionValue_Bucket {
	buckets := make([]*metricspb.DistributionValue_Bucket, len(bounds)+1, len(bounds)+1)
	for i := 0; i <= len(bounds); i++ {
		buckets[i] = &metricspb.DistributionValue_Bucket{}
	}
	for _, val := range values {
		index := getBucketIndex(val, bounds)
		buckets[index].Count++
	}
	return buckets
}

func getBucketIndex(val float64, bounds []float64) int {
	if val < bounds[0] {
		return 0
	}
	for i, lowerBound := range bounds {
		if i < len(bounds)-1 && val >= lowerBound && val < bounds[i+1] {
			return i + 1
		}
	}
	return len(bounds)
}

func formatBuckets(distribution []int64) []*metricspb.DistributionValue_Bucket {
	buckets := make([]*metricspb.DistributionValue_Bucket, len(distribution), len(distribution))
	for i := 0; i < len(distribution); i++ {
		buckets[i] = &metricspb.DistributionValue_Bucket{
			Count: distribution[i],
		}
	}

	return buckets
}

// MakeLabelValue generates a proto representation of a metric label with value as its value.
func MakeLabelValue(value string) *metricspb.LabelValue {
	return &metricspb.LabelValue{
		Value:    value,
		HasValue: true,
	}
}

// MakeSingleValueDistributionTimeSeries generates a proto representation of a timeseries
// containing a single point consisting of a distribution containing a single value.
// The distribution bucket bounds are defined by the bucketOptions argument.
func MakeSingleValueDistributionTimeSeries(
	val float64,
	startTime, currentTime time.Time,
	bucketOptions *metricspb.DistributionValue_BucketOptions,
	labels []*metricspb.LabelValue) *metricspb.TimeSeries {

	bounds := bucketOptions.GetExplicit().Bounds
	distribution := metricspb.DistributionValue{
		Count:                 1,
		Sum:                   val,
		SumOfSquaredDeviation: 0,
		BucketOptions:         bucketOptions,
		Buckets:               MakeBuckets([]float64{val}, bounds),
	}

	return makeTimeseriesFromDistribution(&distribution, startTime, currentTime, labels)
}

// GetSumOfSquaredDeviationsFromIntDist calculates the sum of squared deviations from the mean.
// For values x_i this is:     Sum[i=1..n]((x_i - mean)^2)
// Calculated from the count, sum, and sum of squares of the values.
func GetSumOfSquaredDeviationsFromIntDist(sum, sumSquares, count int64) float64 {
	if count <= 0 {
		return 0
	}

	diff := count*sumSquares - sum*sum
	return float64(diff) / float64(count)
}

// MakeDistributionTimeSeries formats a distribution and its metadata as a TimeSeries.
func MakeDistributionTimeSeries(
	distribution []int64,
	sum float64,
	sumSquaredDeviation float64,
	count int64,
	startTime, currentTime time.Time,
	bucketOptions *metricspb.DistributionValue_BucketOptions,
	labels []*metricspb.LabelValue) *metricspb.TimeSeries {

	distributionProto := metricspb.DistributionValue{
		Count:                 count,
		Sum:                   sum,
		SumOfSquaredDeviation: sumSquaredDeviation,
		BucketOptions:         bucketOptions,
		Buckets:               formatBuckets(distribution),
	}

	return makeTimeseriesFromDistribution(
		&distributionProto,
		startTime, currentTime,
		labels)
}

// makeTimeseriesFromDistribution formats a distribution proto as a TimeSeries.
func makeTimeseriesFromDistribution(
	distribution *metricspb.DistributionValue,
	startTime, currentTime time.Time,
	labels []*metricspb.LabelValue) *metricspb.TimeSeries {
	point := metricspb.Point{
		Timestamp: timestamp.New(currentTime),
		Value:     &metricspb.Point_DistributionValue{DistributionValue: distribution},
	}

	return &metricspb.TimeSeries{
		StartTimestamp: timestamp.New(startTime),
		LabelValues:    labels,
		Points:         []*metricspb.Point{&point},
	}
}
