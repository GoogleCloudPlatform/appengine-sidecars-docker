package vmimageagereceiver

import (
	metricspb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"
)

var vmImageAgeMetric = &metricspb.MetricDescriptor{
	Name:        "vm_image_ages",
	Description: "The VM image age for the VM instance",
	Unit:        "Days",
	Type:        metricspb.MetricDescriptor_GAUGE_DISTRIBUTION,
	LabelKeys:   nil,
}

var vmImageErrorMetric = &metricspb.MetricDescriptor{
	Name:        "vm_image_age_error",
	Description: "The current number of VM instances with errors exporting the VM image age.",
	Unit:        "Count",
	Type:        metricspb.MetricDescriptor_GAUGE_INT64,
	LabelKeys:   nil,
}

var (
	numBounds  = float64(8)
	boundsBase = float64(2)
)
