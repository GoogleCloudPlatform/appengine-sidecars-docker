package dockerstats

import (
	"context"
	"fmt"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/config"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/receiver/receiverhelper"
)

const typeStr = "dockerstats"

// CreateDefaultConfig creates the default configuration for dockerstats receiver.
func createDefaultConfig() config.Receiver {
	return &Config{
		ReceiverSettings: config.NewReceiverSettings(config.NewID(typeStr)),
		ScrapeInterval:   time.Minute,
	}
}

// CreateMetricsReceiver creates a metric receiver for dockerstats.
func createMetricsReceiver(ctx context.Context, settings component.ReceiverCreateSettings, cfg config.Receiver, nextConsumer consumer.Metrics) (component.MetricsReceiver, error) {
	c := cfg.(*Config)
	if c.ScrapeInterval <= 0 {
		return nil, fmt.Errorf("invalid scrape duration: %v, must be positive", c.ScrapeInterval)
	}

	s, err := newScraper(c.ScrapeInterval, nextConsumer, settings.Logger)
	if err != nil {
		return nil, fmt.Errorf("failed to create dockerstats scraper: %v", err)
	}

	return &Receiver{scraper: s}, nil
}

// NewFactory creates and returns a factory for the docker stats receiver.
func NewFactory() component.ReceiverFactory {
	return receiverhelper.NewFactory(
		typeStr,
		createDefaultConfig,
		receiverhelper.WithMetrics(createMetricsReceiver))
}
