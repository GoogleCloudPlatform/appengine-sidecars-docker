module github.com/googlecloudplatform/appengine-sidecars-docker/opentelemetry_collector

replace contrib.go.opencensus.io/exporter/stackdriver => github.com/imccarten1/opencensus-go-exporter-stackdriver v0.13.1

go 1.14

require (
	github.com/census-instrumentation/opencensus-proto v0.2.1
	github.com/golang/protobuf v1.3.3
	github.com/open-telemetry/opentelemetry-collector v0.2.6
	github.com/open-telemetry/opentelemetry-collector-contrib/exporter/stackdriverexporter v0.0.0-20200222201956-5253a327503b
	go.uber.org/zap v1.14.0
)
