package nginxreceiver

import (
	"context"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/config/configerror"
	"go.opentelemetry.io/collector/config/configmodels"
	"go.opentelemetry.io/collector/consumer"
)

const (
	typeStr = "nginxstats"
)

// Factory is the factory for the nginx stats receiver.
type Factory struct {
}

// Type gets the type of the Receiver config created by this factory.
func (f *Factory) Type() configmodels.Type {
	return typeStr
}

// CustomUnmarshaler returns a custom unmarshaler for this config.
// Returning nil means that this receiver does not use one.
func (f *Factory) CustomUnmarshaler() component.CustomUnmarshaler {
	return nil
}

// CreateDefaultConfig creates the default configuration for the receiver.
func (f *Factory) CreateDefaultConfig() configmodels.Receiver {
	return &Config{
		ReceiverSettings: configmodels.ReceiverSettings{
			TypeVal: typeStr,
			NameVal: typeStr,
		},
		ExportInterval: time.Minute,
	}
}

// CreateTraceReceiver generates an error because this receiver does not
// produce traces.
func (f *Factory) CreateTraceReceiver(
	ctx context.Context,
	params component.ReceiverCreateParams,
	cfg configmodels.Receiver,
	nextConsumer consumer.TraceConsumer,
) (component.TraceReceiver, error) {
	return nil, configerror.ErrDataTypeIsNotSupported
}

// CreateMetricsReceiver creates a metrics receiver based on the provided config.
func (f *Factory) CreateMetricsReceiver(
	ctx context.Context,
	params component.ReceiverCreateParams,
	config configmodels.Receiver,
	consumer consumer.MetricsConsumer,
) (component.MetricsReceiver, error) {

	cfg := config.(*Config)
	collector, err := NewNginxStatsCollector(cfg.ExportInterval, cfg.StatsUrl, params.Logger, consumer)

	if err != nil {
		return nil, err
	}

	receiver := &Receiver{
		nginxStatsCollector: collector,
	}

	return receiver, nil
}
