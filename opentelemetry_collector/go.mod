module github.com/googlecloudplatform/appengine-sidecars-docker/opentelemetry_collector

go 1.14

require (
	contrib.go.opencensus.io/exporter/jaeger v0.1.1-0.20190430175949-e8b55949d948 // indirect
	contrib.go.opencensus.io/exporter/ocagent v0.7.0 // indirect
	github.com/census-instrumentation/opencensus-proto v0.3.0
	github.com/docker/distribution v2.7.1+incompatible // indirect
	github.com/docker/docker v17.12.0-ce-rc1.0.20200706150819-a40b877fbb9e+incompatible
	github.com/go-lintpack/lintpack v0.5.2 // indirect
	github.com/golang/protobuf v1.4.2
	github.com/hashicorp/go-plugin v1.3.0 // indirect
	github.com/hashicorp/golang-lru v0.5.4 // indirect
	github.com/open-telemetry/opentelemetry-collector-contrib/exporter/stackdriverexporter v0.14.0
	github.com/open-telemetry/opentelemetry-proto v0.4.0 // indirect
	github.com/ory/x v0.0.109 // indirect
	github.com/securego/gosec v0.0.0-20200316084457-7da9f46445fd // indirect
	github.com/stretchr/testify v1.6.1
	go.opentelemetry.io/collector v0.14.0
	go.uber.org/zap v1.16.0
	golang.org/x/sys v0.0.0-20200905004654-be1d3432aa8f
	sigs.k8s.io/structured-merge-diff v0.0.0-20190525122527-15d366b2352e // indirect
	sourcegraph.com/sqs/pbtypes v0.0.0-20180604144634-d3ebe8f20ae4 // indirect
)
