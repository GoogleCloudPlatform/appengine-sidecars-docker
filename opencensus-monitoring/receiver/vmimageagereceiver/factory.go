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

type Factory struct {
}

func (f *Factory) Type() string {
	return typeStr
}

func (f *Factory) CustomUnmarshaler() receiver.CustomUnmarshaler {
	return nil
}

func (f *Factory) CreateDefaultConfig() configmodels.Receiver {
	return &Config{
		ReceiverSettings: configmodels.ReceiverSettings{
			TypeVal: typeStr,
			NameVal: typeStr,
		},
	}
}

func (f *Factory) CreateTraceReceiver(
	ctx context.Context,
	logger *zap.Logger,
	cfg configmodels.Receiver,
	nextConsumer consumer.TraceConsumer,
) (receiver.TraceReceiver, error) {
	return nil, configerror.ErrDataTypeIsNotSupported
}

func (f *Factory) CreateMetricsReceiver(
	logger *zap.Logger,
	config configmodels.Receiver,
	consumer consumer.MetricsConsumer,
) (receiver.MetricsReceiver, error) {

	cfg := config.(*Config)
	collector := NewVmImageAgeCollector(cfg.ExportInterval, cfg.BuildDate, cfg.VmImageName, consumer)

	receiver := &Receiver{
		vmImageAgeCollector: collector,
	}
	return receiver, nil
}
