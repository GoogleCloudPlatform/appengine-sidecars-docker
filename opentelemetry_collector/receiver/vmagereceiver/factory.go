package vmagereceiver

import (
	"context"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/config"
	"go.opentelemetry.io/collector/consumer"
)

const (
	typeStr = "vmage"
)

// CreateDefaultConfig creates the default configuration for the receiver.
func createDefaultConfig() config.Receiver {
	return &Config{
		ReceiverSettings: config.NewReceiverSettings(config.NewComponentID(typeStr)),
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
	collector := NewVMAgeCollector(cfg.ExportInterval, cfg.BuildDate, cfg.VMImageName, cfg.VMStartTime, cfg.VMReadyTime, consumer, params.Logger)

	receiver := &Receiver{
		vmAgeCollector: collector,
	}
	return receiver, nil
}

// NewFactory creates and returns a factory for the vm age receiver.
func NewFactory() component.ReceiverFactory {
	return component.NewReceiverFactory(
		typeStr,
		createDefaultConfig,
		component.WithMetricsReceiver(createMetricsReceiver))
}
