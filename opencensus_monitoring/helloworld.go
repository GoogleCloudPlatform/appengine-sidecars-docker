// Sample helloworld is a basic App Engine flexible app.
package main

import (
	"fmt"
	"log"
	"math/rand"
	"net/http"
	"os"

	"appengine-sidecars-docker/opencensus_monitoring/exporter"
	"appengine-sidecars-docker/opencensus_monitoring/metric_gatherer"
)

func main() {
	http.HandleFunc("/", handle)
	http.HandleFunc("/_ah/health", healthCheckHandler)
	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}
	metricExporter := exporter.SetupExporter()
	metric_gatherer.RegisterMetrics()

	log.Printf("Listening on port %s", port)
	if err := http.ListenAndServe(":"+port, nil); err != nil {
		metricExporter.Flush()
		metricExporter.StopMetricsExporter()
		log.Fatal(err)
	}
}

func handle(w http.ResponseWriter, r *http.Request) {
	if r.URL.Path != "/" {
		http.NotFound(w, r)
		return
	}
	for i := 0; i < 5; i++ {
		metric_gatherer.RecordTestLatency(rand.Float64() * float64(3000))
		//metric_gatherer.RecordClosedConnections(int64(1))
	}
	fmt.Fprint(w, "Hello world!")
}

func healthCheckHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Fprint(w, "ok")
}

