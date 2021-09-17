package vmagereceiver

import (
	"time"

	"go.opentelemetry.io/collector/config"
)

// Config defines the configuration for the VM age receiver.
type Config struct {
	config.ReceiverSettings `mapstructure:",squash"`
	ExportInterval          time.Duration `mapstructure:"export_interval"`
	BuildDate               string        `mapstructure:"build_date"`
	VMImageName             string        `mapstructure:"vm_image_name"`
	VMStartTime             string        `mapstructure:"vm_start_time"`
	VMReadyTime             string        `mapstructure:"vm_ready_time"`
}
