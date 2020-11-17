package vmimageagereceiver

import (
	"context"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/config/configmodels"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/receiver/receiverhelper"
)

const (
	typeStr = "vmimageage"
)

// CreateDefaultConfig creates the default configuration for the receiver.
func createDefaultConfig() configmodels.Receiver {
	return &Config{
		ReceiverSettings: configmodels.ReceiverSettings{
			TypeVal: typeStr,
			NameVal: typeStr,
		},
	}
}

// CreateMetricsReceiver creates a metrics receiver based on the provided config.
func createMetricsReceiver(
	ctx context.Context,
	params component.ReceiverCreateParams,
	config configmodels.Receiver,
	consumer consumer.MetricsConsumer,
) (component.MetricsReceiver, error) {

	cfg := config.(*Config)
	collector := NewVMImageAgeCollector(cfg.ExportInterval, cfg.BuildDate, cfg.VMImageName, consumer, params.Logger)

	receiver := &Receiver{
		vmImageAgeCollector: collector,
	}
	return receiver, nil
}

func NewFactory() component.ReceiverFactory {
	return receiverhelper.NewFactory(
		typeStr,
		createDefaultConfig,
		receiverhelper.WithMetrics(createMetricsReceiver))
}
