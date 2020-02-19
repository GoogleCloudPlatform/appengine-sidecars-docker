package vmimageagereceiver

import (
	"sync"

	"github.com/open-telemetry/opentelemetry-collector/receiver"
)

// Receiver is the type that provides Receiver functionaly for the VM image age metrics.
type Receiver struct {
	vmImageAgeCollector *VMImageAgeCollector

	stopOnce  sync.Once
	startOnce sync.Once
}

const metricsSource string = "VMImageAgeReceiver"

// MetricsSource gets the receiver type.
func (receiver *Receiver) MetricsSource() string {
	return metricsSource
}

// StartMetricsReception starts the underlying VM metrics generator.
func (receiver *Receiver) StartMetricsReception(host receiver.Host) error {
	receiver.startOnce.Do(func() {
		receiver.vmImageAgeCollector.StartCollection()
	})
	return nil
}

// StopMetricsReception stops and cancels the underlying VM metrics generator.
func (receiver *Receiver) StopMetricsReception() error {
	receiver.stopOnce.Do(func() {
		receiver.vmImageAgeCollector.StopCollection()
	})
	return nil
}
