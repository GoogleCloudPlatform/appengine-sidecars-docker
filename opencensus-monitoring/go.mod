module github.com/googlecloudplatform/appengine-sidecars-docker/opencensus-monitoring

go 1.13

require (
	contrib.go.opencensus.io/resource v0.1.2
	github.com/GoogleCloudPlatform/appengine-sidecars-docker v0.0.0-20191008180841-97c09a40bf9b // indirect
	github.com/census-instrumentation/opencensus-proto v0.2.1
	github.com/golang/protobuf v1.3.2
	github.com/open-telemetry/opentelemetry-collector v0.2.1-0.20191016224815-dfabfb0c1d1e
	github.com/open-telemetry/opentelemetry-collector-contrib/exporter/stackdriverexporter v0.0.0-20191203223618-d7af801adf6e
	github.com/stretchr/testify v1.4.0
	go.uber.org/zap v1.13.0
)
