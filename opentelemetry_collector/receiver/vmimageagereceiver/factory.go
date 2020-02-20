package vmimageagereceiver

import (
	"context"

	"go.uber.org/zap"

	"github.com/open-telemetry/opentelemetry-collector/config/configerror"
	"github.com/open-telemetry/opentelemetry-collector/config/configmodels"
	"github.com/open-telemetry/opentelemetry-collector/consumer"
	"github.com/open-telemetry/opentelemetry-collector/receiver"
)

const (
	typeStr = "vmimageage"
)

// Factory is the factory for the VM image age receiver.
type Factory struct {
}

// Type gets the type of the Receiver config created by this factory.
func (f *Factory) Type() string {
	return typeStr
}

// Type gets the type of the Exporter config created by this factory.
func (f *Factory) CustomUnmarshaler() receiver.CustomUnmarshaler {
	return nil
}

// CreateDefaultConfig creates the default configuration for the receiver.
func (f *Factory) CreateDefaultConfig() configmodels.Receiver {
	return &Config{
		ReceiverSettings: configmodels.ReceiverSettings{
			TypeVal: typeStr,
			NameVal: typeStr,
		},
	}
}

// CreateTraceReceiver generates an error because this receiver does not
// produce traces.
func (f *Factory) CreateTraceReceiver(
	ctx context.Context,
	logger *zap.Logger,
	cfg configmodels.Receiver,
	nextConsumer consumer.TraceConsumer,
) (receiver.TraceReceiver, error) {
	return nil, configerror.ErrDataTypeIsNotSupported
}

// CreateMetricsReceiver creates a metrics receiver based on the provided config.
func (f *Factory) CreateMetricsReceiver(
	logger *zap.Logger,
	config configmodels.Receiver,
	consumer consumer.MetricsConsumer,
) (receiver.MetricsReceiver, error) {

	cfg := config.(*Config)
	collector := NewVMImageAgeCollector(cfg.ExportInterval, cfg.BuildDate, cfg.VMImageName, consumer)

	receiver := &Receiver{
		vmImageAgeCollector: collector,
	}
	return receiver, nil
}
