package vmimageagereceiver

import (
	"time"

	"github.com/open-telemetry/opentelemetry-collector/config/configmodels"
)

type Config struct {
	configmodels.ReceiverSettings `mapstructure:",squash"`
	ExportInterval                time.Duration `mapstructure:"export_interval"`
	BuildDate                     string         `mapstructure:"build_date"`
}
