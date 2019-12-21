package vmimageagereceiver

import (
	"sync"

	"github.com/open-telemetry/opentelemetry-collector/receiver"
)

type Receiver struct {
	vmImageAgeCollector *VmImageAgeCollector

	stopOnce  sync.Once
	startOnce sync.Once
}

const metricsSource string = "VMImageAgeReceiver"

func (receiver *Receiver) MetricsSource() string {
	return metricsSource
}

func (receiver *Receiver) StartMetricsReception(host receiver.Host) error {
	receiver.startOnce.Do(func() {
		receiver.vmImageAgeCollector.StartCollection()
	})
	return nil
}

func (receiver *Receiver) StopMetricsReception() error {
	receiver.stopOnce.Do(func() {
		receiver.vmImageAgeCollector.StopCollection()
	})
	return nil
}
