package dockerstats

import (
	"context"
	"testing"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/config/configerror"

	"github.com/stretchr/testify/assert"
	"go.opentelemetry.io/collector/config/configcheck"

	"go.uber.org/zap"
)

func TestCreateDefaultConfig(t *testing.T) {
	factory := &Factory{}
	cfg := factory.CreateDefaultConfig()
	assert.NotNil(t, cfg, "failed to create default config")
	assert.NoError(t, configcheck.ValidateConfig(cfg))
	c := cfg.(*Config)
	assert.Greater(t, int64(c.ScrapeInterval), int64(0))
}

func TestCreateTraceReceiver(t *testing.T) {
	factory := &Factory{}
	cfg := factory.CreateDefaultConfig()
	params := component.ReceiverCreateParams{Logger: zap.NewNop()}

	r, err := factory.CreateTraceReceiver(context.Background(), params, cfg, nil)
	assert.Equal(t, err, configerror.ErrDataTypeIsNotSupported)
	assert.Nil(t, r)
}

func TestCreateMetricsReceiver(t *testing.T) {
	factory := &Factory{}
	cfg := factory.CreateDefaultConfig()
	params := component.ReceiverCreateParams{Logger: zap.NewNop()}

	r, err := factory.CreateMetricsReceiver(context.Background(), params, cfg, nil)
	assert.Nil(t, err)
	assert.NotNil(t, r)
}
