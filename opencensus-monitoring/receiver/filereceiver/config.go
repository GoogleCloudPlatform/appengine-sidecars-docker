package filereceiver

import (
	"time"

	"github.com/open-telemetry/opentelemetry-collector/config/configmodels"
)

type Config struct {
	configmodels.ReceiverSettings `mapstructure:",squash"`
	ExportInterval                time.Duration `mapstructure:"export_interval"`
	MetricName                    string        `mapstructure:"metric_name"`
	MetricValue                   int64         `mapstructure:"metric_value"`
}
