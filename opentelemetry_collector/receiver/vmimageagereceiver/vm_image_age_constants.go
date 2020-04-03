package vmimageagereceiver

import (
	metricspb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"
)

var vmImageNameLabel = &metricspb.LabelKey{
	Key:         "vm_image_name",
	Description: "The name of the VM image",
}

var vmImageAgeMetric = &metricspb.MetricDescriptor{
	Name:        "vm_image_ages",
	Description: "The VM image age for the VM instance",
	Unit:        "Days",
	Type:        metricspb.MetricDescriptor_GAUGE_DISTRIBUTION,
	LabelKeys:   []*metricspb.LabelKey{vmImageNameLabel},
}

var vmImageErrorMetric = &metricspb.MetricDescriptor{
	Name:        "vm_image_ages_error",
	Description: "The current number of VM instances with errors exporting the VM image age.",
	Unit:        "Count",
	Type:        metricspb.MetricDescriptor_GAUGE_INT64,
	LabelKeys:   []*metricspb.LabelKey{vmImageNameLabel},
}

var (
	numBounds  = float64(8)
	boundsBase = float64(2)
)
