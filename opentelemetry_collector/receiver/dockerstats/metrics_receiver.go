package dockerstats

import (
	"context"
	"sync"

	"go.opentelemetry.io/collector/component"
)

// Receiver implements component.MetricReceiver.
// Manages the lifecycle of the scraper that scrapes docker stats from the API.
type Receiver struct {
	scraper *scraper

	startOnce sync.Once
	stopOnce  sync.Once
}

// Start tells this receiver to start.
func (r *Receiver) Start(ctx context.Context, host component.Host) error {
	r.startOnce.Do(func() {
		r.scraper.start()
	})
	return nil
}

// Shutdown tells this receiver to stop.
func (r *Receiver) Shutdown(ctx context.Context) error {
	r.stopOnce.Do(func() {
		r.scraper.stop()
	})
	return nil
}
