package vmagereceiver

import (
	"context"
	"testing"

	"github.com/stretchr/testify/assert"
	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/component/componenterror"
	"go.opentelemetry.io/collector/config/configtest"
	"go.uber.org/zap"
)

func TestCreateDefaultConfig(t *testing.T) {
	factory := NewFactory()
	cfg := factory.CreateDefaultConfig()
	assert.NotNil(t, cfg, "failed to create default config")
	assert.NoError(t, configtest.CheckConfigStruct(cfg))
}

func TestCreateReceiver(t *testing.T) {
	factory := NewFactory()
	cfg := factory.CreateDefaultConfig()
	params := component.ReceiverCreateSettings{
		TelemetrySettings: component.TelemetrySettings{
			Logger: zap.NewNop(),
		},
	}

	tReceiver, err := factory.CreateTracesReceiver(context.Background(), params, cfg, nil)

	assert.Equal(t, err, componenterror.ErrDataTypeIsNotSupported)
	assert.Nil(t, tReceiver)

	mReceiver, err := factory.CreateMetricsReceiver(context.Background(), params, cfg, nil)

	assert.Nil(t, err)
	assert.NotNil(t, mReceiver)
}
