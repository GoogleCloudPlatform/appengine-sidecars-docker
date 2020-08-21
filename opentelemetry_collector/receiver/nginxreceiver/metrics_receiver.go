package nginxreceiver

import (
	"context"
	"sync"

	"go.opentelemetry.io/collector/component"
)

// Receiver is the type that provides Receiver functionality for the nginx stats metrics.
type Receiver struct {
	nginxStatsCollector *NginxStatsCollector

	stopOnce  sync.Once
	startOnce sync.Once
}

// Start starts the underlying nginx metrics generator.
func (receiver *Receiver) Start(ctx context.Context, host component.Host) error {
	receiver.startOnce.Do(func() {
		receiver.nginxStatsCollector.StartCollection()
	})
	return nil
}

// Shutdown stops and cancels the underlying nginx metrics generator.
func (receiver *Receiver) Shutdown(ctx context.Context) error {
	receiver.stopOnce.Do(func() {
		receiver.nginxStatsCollector.StopCollection()
	})
	return nil
}
