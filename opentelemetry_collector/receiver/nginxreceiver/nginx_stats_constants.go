package nginxreceiver

import (
	metricspb "github.com/census-instrumentation/opencensus-proto/gen-go/metrics/v1"
)

var requestLatencyMetric = &metricspb.MetricDescriptor{
	Name:        "on_vm_request_latencies",
	Description: "The request latency measured at nginx. Includes latency from nginx and the user's app code",
	Unit:        "milliseconds",
	Type:        metricspb.MetricDescriptor_CUMULATIVE_DISTRIBUTION,
	LabelKeys:   []*metricspb.LabelKey{},
}

var upstreamLatencyMetric = &metricspb.MetricDescriptor{
	Name:        "on_vm_upstream_latencies",
	Description: "The upstream latency measured at nginx. ie The latency of the user provided app code.",
	Unit:        "milliseconds",
	Type:        metricspb.MetricDescriptor_CUMULATIVE_DISTRIBUTION,
	LabelKeys:   []*metricspb.LabelKey{},
}

var websocketLatencyMetric = &metricspb.MetricDescriptor{
	Name:        "web_socket/durations",
	Description: "The duration of websocket connections measured at nginx.",
	Unit:        "milliseconds",
	Type:        metricspb.MetricDescriptor_CUMULATIVE_DISTRIBUTION,
	LabelKeys:   []*metricspb.LabelKey{},
}
