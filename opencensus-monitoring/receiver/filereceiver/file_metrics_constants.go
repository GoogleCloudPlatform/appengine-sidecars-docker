package filereceiver

import (
	metricspb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"
)

var testFileMetric = &metricspb.MetricDescriptor{
	Name:        "local/open_telemetry_collector_test",
	Description: "Test open telemetry metric",
	Unit:        "Count",
	Type:        metricspb.MetricDescriptor_GAUGE_INT64,
	LabelKeys:   nil,
}
