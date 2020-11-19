package dockerstats

import (
	"context"
	"fmt"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/config/configmodels"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/receiver/receiverhelper"
)

const typeStr = "dockerstats"

// CreateDefaultConfig creates the default configuration for dockerstats receiver.
func createDefaultConfig() configmodels.Receiver {
	return &Config{
		ReceiverSettings: configmodels.ReceiverSettings{
			TypeVal: typeStr,
			NameVal: typeStr,
		},
		ScrapeInterval: time.Minute,
	}
}

// CreateMetricsReceiver creates a metric receiver for dockerstats.
func createMetricsReceiver(ctx context.Context, params component.ReceiverCreateParams, cfg configmodels.Receiver, nextConsumer consumer.MetricsConsumer) (component.MetricsReceiver, error) {
	c := cfg.(*Config)
	if c.ScrapeInterval <= 0 {
		return nil, fmt.Errorf("invalid scrape duration: %v, must be positive", c.ScrapeInterval)
	}

	s, err := newScraper(c.ScrapeInterval, nextConsumer, params.Logger)
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
