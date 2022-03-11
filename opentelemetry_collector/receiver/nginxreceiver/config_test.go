package nginxreceiver

import (
	"path"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/component/componenttest"
	"go.opentelemetry.io/collector/config"
	"go.opentelemetry.io/collector/service/servicetest"
)

func TestLoadConfig(t *testing.T) {
	factories, err := componenttest.NopFactories()
	assert.Nil(t, err)

	factory := NewFactory()
	factories.Receivers[typeStr] = factory
	cfg, err := servicetest.LoadConfigAndValidate(path.Join(".", "testdata", "config.yaml"), factories)

	require.NoError(t, err)
	require.NotNil(t, cfg)

	assert.Equal(t, len(cfg.Receivers), 2)

	defaultReceiver := cfg.Receivers[config.NewComponentID("nginxstats")]
	assert.Equal(t, defaultReceiver, factory.CreateDefaultConfig())

	customReceiver := cfg.Receivers[config.NewComponentIDWithName("nginxstats", "customname")]
	assert.Equal(t, customReceiver,
		&Config{
			ReceiverSettings: config.NewReceiverSettings(config.NewComponentIDWithName("nginxstats", "customname")),
			ExportInterval:   10 * time.Minute,
			StatsURL:         "http://example.com",
		})
}
