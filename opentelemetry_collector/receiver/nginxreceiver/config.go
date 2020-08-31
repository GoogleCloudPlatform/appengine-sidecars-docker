package nginxreceiver

import (
	"go.opentelemetry.io/collector/config/configmodels"
	"time"
)

// Config defines the configuration for the nginx stats receiver.
type Config struct {
	configmodels.ReceiverSettings `mapstructure:",squash"`
	ExportInterval                time.Duration `mapstructure:"export_interval"`
	StatsURL                      string        `mapstructure:"stats_url"`
}
