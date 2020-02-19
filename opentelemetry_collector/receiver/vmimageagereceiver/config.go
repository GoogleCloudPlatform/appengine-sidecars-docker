package vmimageagereceiver

import (
	"time"

	"github.com/open-telemetry/opentelemetry-collector/config/configmodels"
)

// Config defines the configuration for the VM image age receiver.
type Config struct {
	configmodels.ReceiverSettings `mapstructure:",squash"`
	ExportInterval                time.Duration `mapstructure:"export_interval"`
	BuildDate                     string        `mapstructure:"build_date"`
	VMImageName                   string        `mapstructure:"vm_image_name"`
}
