package nginxreceiver

import (
	"context"
	"testing"

	"github.com/stretchr/testify/assert"
	"go.uber.org/zap"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/config/configcheck"
	"go.opentelemetry.io/collector/config/configerror"
	"go.opentelemetry.io/collector/config/configmodels"
)

func TestCreateDefaultConfig(t *testing.T) {
	factory := &Factory{}
	cfg := factory.CreateDefaultConfig()
	assert.NotNil(t, cfg, "failed to create default config")
	assert.NoError(t, configcheck.ValidateConfig(cfg))
}

func TestCreateReceiver(t *testing.T) {
	factory := &Factory{}
	cfg := factory.CreateDefaultConfig()
	config := cfg.(*Config)
	config.StatsURL = "http://example.com"
	cfg = configmodels.Receiver(config)
	params := component.ReceiverCreateParams{Logger: zap.NewNop()}

	tReceiver, err := factory.CreateTraceReceiver(context.Background(), params, cfg, nil)

	assert.Equal(t, err, configerror.ErrDataTypeIsNotSupported)
	assert.Nil(t, tReceiver)

	mReceiver, err := factory.CreateMetricsReceiver(context.Background(), params, cfg, nil)

	assert.Nil(t, err)
	assert.NotNil(t, mReceiver)
}
