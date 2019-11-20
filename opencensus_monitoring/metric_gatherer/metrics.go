// Definitions of measures and utility functions to record them.
package metric_gatherer

import (
	"log"

	//"google3/third_party/golang/opencensus/stats/stats"
)

var (
        bounds = []float64{0, 100, 200, 400, 1000, 2000, 4000}
	testLatency = newFloatDistributionMetric("test/latency22", "A test latency value", "ms", []string{"test_label"}, bounds)
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
	testLatency.recordFloat(value, map[string]string{"test_label": "label"})
}

func RecordClosedConnections(value int64) {
	closedConnections.recordInt(value, map[string]string{})
}
