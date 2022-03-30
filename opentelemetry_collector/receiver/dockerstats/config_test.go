package dockerstats

import (
	"path"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
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
	assert.NoError(t, err)

	defaultReceiver := cfg.Receivers[config.NewComponentID("dockerstats")]
	assert.Equal(t, defaultReceiver, factory.CreateDefaultConfig())

	customReceiver := cfg.Receivers[config.NewComponentIDWithName("dockerstats", "customname")]
	assert.Equal(t, customReceiver, &Config{
		ReceiverSettings: config.NewReceiverSettings(config.NewComponentIDWithName("dockerstats", "customname")),
		ScrapeInterval:   10 * time.Minute,
	})
}
