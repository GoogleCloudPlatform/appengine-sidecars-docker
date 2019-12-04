// Definitions of measures and utility functions to record them.
package metric_gatherer

import (
	"log"

	//"google3/third_party/golang/opencensus/stats/stats"
)

var (
        bounds = []float64{0, 100, 200, 400, 1000, 2000, 4000}
	testLatency = newFloatDistributionMetric("test/latency_distribution_new2", "A test latency value", "ms", []string{"test_label", "module_id", "version_id", "project_id", "instance_id", "zone"}, bounds)
	closedConnections = newCountMetric("closed_connections", "closed connections", "count", []string{})
)

func RegisterMetrics() {
        someSuccess := false
	metrics := []metricRegistrar{testLatency}

	for _, metric := range metrics {
		someSuccess = someSuccess || metric.register()
	}
        //someSuccess = someSuccess || registerCount(closedConnections, noTags)

        if !someSuccess {
                log.Fatalf("Failed to register any views")
        }
}

func RecordTestLatency(value float64) {
	testLatency.recordFloat(value, map[string]string{"test_label": "label", "module_id": "default", "verison_id": "20191118t095526", "project_id": "imccarten-flex-test", "instance_id": "aef-default-test-20191118t095526-nlcl", "zone": "australia-southeast1"})
}

func RecordClosedConnections(value int64) {
	closedConnections.recordInt(value, map[string]string{})
}
