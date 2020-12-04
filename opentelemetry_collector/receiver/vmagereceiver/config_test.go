package vmagereceiver

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

	defaultReceiver := cfg.Receivers["vmage"]
	assert.Equal(t, defaultReceiver, factory.CreateDefaultConfig())

	customReceiver := cfg.Receivers["vmage/customname"].(*Config)
	assert.Equal(t, customReceiver,
		&Config{
			ReceiverSettings: configmodels.ReceiverSettings{
				TypeVal: typeStr,
				NameVal: "vmage/customname",
			},
			ExportInterval: 10 * time.Minute,
			BuildDate:      "2006-01-02T15:04:05Z07:00",
			VMImageName:    "test_vm_image_name",
			VMStartTime:    "2007-01-01T01:01:00Z07:00",
			VMReadyTime:    "2007-01-01T01:02:00Z07:00",
		})
}
