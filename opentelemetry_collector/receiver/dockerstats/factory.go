package dockerstats

import (
	"context"
	"fmt"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/config/configerror"
	"go.opentelemetry.io/collector/config/configmodels"
	"go.opentelemetry.io/collector/consumer"
)

const receiverType = "dockerstats"

// Factory implements component.ReceiverFactory interface.
type Factory struct{}

// Type gets the type of receiver created by this factory.
func (f *Factory) Type() configmodels.Type {
	return receiverType
}

// CreateDefaultConfig creates the default configuration for dockerstats receiver.
func (f *Factory) CreateDefaultConfig() configmodels.Receiver {
	return &Config{
		ReceiverSettings: configmodels.ReceiverSettings{
			TypeVal: receiverType,
			NameVal: receiverType,
		},
		ScrapeInterval: 30 * time.Second,
	}
}

// CustomUnmarshaler returns a custom unmarshaler for the configuration or nil if no
// custom unmarshaling is required.
func (f *Factory) CustomUnmarshaler() component.CustomUnmarshaler {
	return nil
}

// CreateTraceReceiver creates a trace receiver for dockerstats.
// This returns an error because we don't support tracing in dockerstats.
func (f *Factory) CreateTraceReceiver(ctx context.Context, params component.ReceiverCreateParams, cfg configmodels.Receiver, nextConsumer consumer.TraceConsumer) (component.TraceReceiver, error) {
	return nil, configerror.ErrDataTypeIsNotSupported
}

// CreateLogsReceiver generates an error because this receiver does not
// produce traces.
func (f* Factory) CreateLogsReceiver(
        _ context.Context,
        _ component.ReceiverCreateParams,
        cfg configmodels.Receiver,
        nextConsumer consumer.LogsConsumer,
) (component.LogsReceiver, error) {
        return nil, configerror.ErrDataTypeIsNotSupported
}

// CreateMetricsReceiver creates a metric receiver for dockerstats.
func (f *Factory) CreateMetricsReceiver(ctx context.Context, params component.ReceiverCreateParams, cfg configmodels.Receiver, nextConsumer consumer.MetricsConsumer) (component.MetricsReceiver, error) {
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
