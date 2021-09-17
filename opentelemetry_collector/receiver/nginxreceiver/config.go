package nginxreceiver

import (
	"time"

	"go.opentelemetry.io/collector/config"
)

// Config defines the configuration for the nginx stats receiver.
type Config struct {
	config.ReceiverSettings `mapstructure:",squash"`
	ExportInterval          time.Duration `mapstructure:"export_interval"`
	StatsURL                string        `mapstructure:"stats_url"`
}
