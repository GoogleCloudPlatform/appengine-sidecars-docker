package nginxreceiver

import (
	"path"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"go.opentelemetry.io/collector/component/componenttest"
	"go.opentelemetry.io/collector/config/configmodels"
	"go.opentelemetry.io/collector/config/configtest"
)

func TestLoadConfig(t *testing.T) {
	factories, err := componenttest.ExampleComponents()
	assert.Nil(t, err)

	factory := NewFactory()
	factories.Receivers[typeStr] = factory
	cfg, err := configtest.LoadConfigFile(t, path.Join(".", "testdata", "config.yaml"), factories)

	require.NoError(t, err)
	require.NotNil(t, cfg)

	assert.Equal(t, len(cfg.Receivers), 2)

	defaultReceiver := cfg.Receivers["nginxstats"]
	assert.Equal(t, defaultReceiver, factory.CreateDefaultConfig())

	customReceiver := cfg.Receivers["nginxstats/customname"].(*Config)
	assert.Equal(t, customReceiver,
		&Config{
			ReceiverSettings: configmodels.ReceiverSettings{
				TypeVal: typeStr,
				NameVal: "nginxstats/customname",
			},
			ExportInterval: 10 * time.Minute,
			StatsURL:       "http://example.com",
		})
}
