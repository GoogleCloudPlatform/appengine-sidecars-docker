package filereceiver

import (
	"sync"
	"log"

	"github.com/open-telemetry/opentelemetry-collector/receiver"
)

type Receiver struct {
	fmc *FileMetricsCollector

	stopOnce  sync.Once
	startOnce sync.Once
}

const metricsSource string = "FileMetrics"

func (fmr *Receiver) MetricsSource() string {
	return metricsSource
}

func (fmr *Receiver) StartMetricsReception(host receiver.Host) error {
	log.Printf("in StartMetricsReception")
	fmr.startOnce.Do(func() {
		log.Printf("Starting metrics reception")
		fmr.fmc.StartCollection()
	})
	return nil
}

func (fmr *Receiver) StopMetricsReception() error {
	fmr.stopOnce.Do(func() {
		fmr.fmc.StopCollection()
	})
	return nil
}
