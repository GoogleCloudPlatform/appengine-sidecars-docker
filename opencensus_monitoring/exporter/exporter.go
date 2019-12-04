package exporter

import (
	"log"
	"time"

	//"google3/third_party/golang/opencensus/stats/stats"
	//"google3/third_party/golang/opencensus/stats/view/view"
	//"google3/third_party/golang/opencensus_stackdriver/stackdriver"
	"contrib.go.opencensus.io/exporter/stackdriver"
	//"contrib.go.opencensus.io/exporter/stackdriver/monitoredresource"
)

type GaeInstance struct {
	ProjectID string
	ModuleID string
	VersionID string
	InstanceID string
	Location string
}

func (gae *GaeInstance) MonitoredResource() (resType string, labels map[string]string) {
	labels = map[string]string{
		"project_id": gae.ProjectID,
		"module_id": gae.ModuleID,
		"version_id": gae.VersionID,
		"instance_id": gae.InstanceID,
		"location": gae.Location,
	}
	return "gae_instance", labels
}

func SetupExporter() *stackdriver.Exporter {
	//gae := &GaeInstance{
	//	ProjectID: "imccarten-flex-test",
	//	ModuleID: "default",
	//	VersionID: "20191118t095526",
	//	InstanceID: "aef-default-test-20191118t095526-nlcl",
	//	Location: "australia-southeast1",
	//}
	
	//gce := &monitoredresource.GCEInstance{
	//	ProjectID: "imccarten-flex-test",
	//	InstanceID: "aef-default-test-20191118t095526-nlcl",
	//	Zone: "australia-southeast1",
	//}

	labels := &stackdriver.Labels{}
	//labels.Set("module_id", "default", "The service/module name")
	//labels.Set("version_id", "20191118t095526", "The version name")

	sdExporter, err := stackdriver.NewExporter(stackdriver.Options{
		ProjectID:         "imccarten-flex-test",
		MetricPrefix:      "flex/local",
		ReportingInterval: 60 * time.Second,
		//MonitoredResource: gce,
		DefaultMonitoringLabels: labels,
	})
	if err != nil {
		log.Fatalf("Failed to create Stackdriver exporter: %v", err)
	}

	sdExporter.StartMetricsExporter()
	return sdExporter
}

