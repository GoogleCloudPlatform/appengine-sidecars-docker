package vmagereceiver

import (
	"context"
	"sync"

	"go.opentelemetry.io/collector/component"
)

// Receiver is the type that provides Receiver functionaly for the VM age metrics.
type Receiver struct {
	vmAgeCollector *VMAgeCollector

	stopOnce  sync.Once
	startOnce sync.Once
}

// Start starts the underlying VM metrics generator.
func (receiver *Receiver) Start(ctx context.Context, host component.Host) error {
	receiver.startOnce.Do(func() {
		receiver.vmAgeCollector.StartCollection()
	})
	return nil
}

// Shutdown stops and cancels the underlying VM metrics generator.
func (receiver *Receiver) Shutdown(ctx context.Context) error {
	receiver.stopOnce.Do(func() {
		receiver.vmAgeCollector.StopCollection()
	})
	return nil
}
