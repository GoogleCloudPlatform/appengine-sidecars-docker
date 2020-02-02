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

func MakeInt64TimeSeries(val int64, startTime, now time.Time, labels []*metricspb.LabelValue) *metricspb.TimeSeries {
	return &metricspb.TimeSeries{
		StartTimestamp: TimeToTimestamp(startTime),
		LabelValues:    labels,
		Points:         []*metricspb.Point{{Timestamp: TimeToTimestamp(now), Value: &metricspb.Point_Int64Value{Int64Value: val}}},
	}
}

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

func MakeBuckets(values, bounds []float64) []*metricspb.DistributionValue_Bucket {
	buckets := make([]*metricspb.DistributionValue_Bucket, len(bounds)+1, len(bounds)+1)
	for i := 0; i <= len(bounds); i++ {
		buckets[i] = &metricspb.DistributionValue_Bucket{}
	}
	for _, val := range values {
		index := GetBucketIndex(val, bounds)
		buckets[index].Count += 1
	}
	return buckets
}

func GetBucketIndex(val float64, bounds []float64) int {
	if val < bounds[0] {
		return 0
	}
	for i, lower_bound := range bounds {
		if i < len(bounds)-1 && val >= lower_bound && val < bounds[i+1] {
			return i + 1
		}
	}
	return len(bounds)
}

func MakeLabelValue(value string) *metricspb.LabelValue {
	return &metricspb.LabelValue{
		Value:    value,
		HasValue: true,
	}
}

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
