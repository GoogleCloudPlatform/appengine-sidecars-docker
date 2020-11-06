module github.com/googlecloudplatform/appengine-sidecars-docker/opentelemetry_collector

go 1.14

require (
	github.com/Azure/go-autorest v11.2.8+incompatible // indirect
	github.com/census-instrumentation/opencensus-proto v0.3.0
	github.com/docker/distribution v2.7.1+incompatible // indirect
	github.com/docker/docker v1.13.1
	github.com/golang/protobuf v1.4.2
	github.com/hashicorp/golang-lru v0.5.4 // indirect
	github.com/open-telemetry/opentelemetry-collector-contrib/exporter/stackdriverexporter v0.8.0
	github.com/ory/x v0.0.109 // indirect
	github.com/shirou/gopsutil v2.20.4+incompatible // indirect
	github.com/stretchr/testify v1.6.1
	go.opentelemetry.io/collector v0.9.0
	go.uber.org/zap v1.15.0
	sigs.k8s.io/structured-merge-diff v0.0.0-20190525122527-15d366b2352e // indirect
)
