package nginxreceiver

import (
	"context"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/config"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/receiver/receiverhelper"
)

const (
	typeStr = "nginxstats"
)

// CreateDefaultConfig creates the default configuration for the receiver.
func createDefaultConfig() config.Receiver {
	return &Config{
		ReceiverSettings: config.NewReceiverSettings(config.NewID(typeStr)),
		ExportInterval:   time.Minute,
	}
}

// CreateMetricsReceiver creates a metrics receiver based on the provided config.
func createMetricsReceiver(
	ctx context.Context,
	params component.ReceiverCreateSettings,
	config config.Receiver,
	consumer consumer.Metrics,
) (component.MetricsReceiver, error) {

	cfg := config.(*Config)
	collector, err := NewNginxStatsCollector(cfg.ExportInterval, cfg.StatsURL, params.Logger, consumer)

	if err != nil {
		return nil, err
	}

	receiver := &Receiver{
		nginxStatsCollector: collector,
	}

	return receiver, nil
}

// NewFactory creates and returns a factory for the nginx receiver.
func NewFactory() component.ReceiverFactory {
	return receiverhelper.NewFactory(
		typeStr,
		createDefaultConfig,
		receiverhelper.WithMetrics(createMetricsReceiver))
}
