module github.com/googlecloudplatform/appengine-sidecars-docker/opentelemetry_collector

go 1.14

require (
	github.com/census-instrumentation/opencensus-proto v0.2.1
	github.com/docker/distribution v2.7.1+incompatible // indirect
	github.com/docker/docker v1.13.1
	github.com/golang/glog v0.0.0-20160126235308-23def4e6c14b
	github.com/golang/protobuf v1.3.5
	github.com/hashicorp/golang-lru v0.5.4 // indirect
	github.com/open-telemetry/opentelemetry-collector-contrib/exporter/stackdriverexporter v0.6.0
	github.com/stretchr/testify v1.6.1
	go.opentelemetry.io/collector v0.6.0
	go.uber.org/zap v1.14.0
)
