package metricgenerator

import (
	"math"
	"time"

	metricspb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"

	"github.com/golang/protobuf/ptypes/timestamp"
)

// TimeToTimestamp converts a time.Time to a timestamp.Timestamp pointer.
func TimeToTimestamp(t time.Time) *timestamp.Timestamp {
	if t.IsZero() {
		return nil
	}
	nanoTime := t.UnixNano()
	return &timestamp.Timestamp{
		Seconds: nanoTime / 1e9,
		Nanos:   int32(nanoTime % 1e9),
	}
}

// MakeInt64TimeSeries generates a proto representation of a timeseries containing a single point for an int64 metric.
func MakeInt64TimeSeries(val int64, startTime, now time.Time, labels []*metricspb.LabelValue) *metricspb.TimeSeries {
	return &metricspb.TimeSeries{
		StartTimestamp: TimeToTimestamp(startTime),
		LabelValues:    labels,
		Points:         []*metricspb.Point{{Timestamp: TimeToTimestamp(now), Value: &metricspb.Point_Int64Value{Int64Value: val}}},
	}
}

// MakeExponentialBucketOptions generates a proto representation of a config which,
// defines a distribution's bounds. This defines maxExponent + 1 (= N) buckets. The boundaries for bucket
// index i are:
//
// [0, boundsBase ^ i) for i == 0
// [boundsBase ^ (i - 1)], boundsBase ^ i) for 0 < i < N-1
// [boundsBase ^ i, +infinity) for i == N-1 == maxExponent
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

	point := metricspb.Point{
		Timestamp: TimeToTimestamp(currentTime),
		Value:     &metricspb.Point_DistributionValue{DistributionValue: &distribution},
	}

	return &metricspb.TimeSeries{
		StartTimestamp: TimeToTimestamp(startTime),
		LabelValues:    labels,
		Points:         []*metricspb.Point{&point},
	}
}
