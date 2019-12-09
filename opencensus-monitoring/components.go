// Copyright 2019 OpenTelemetry Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package main

import (
	"github.com/open-telemetry/opentelemetry-collector/config"
	"github.com/open-telemetry/opentelemetry-collector/defaults"
	"github.com/open-telemetry/opentelemetry-collector/exporter"
	"github.com/open-telemetry/opentelemetry-collector/oterr"
	"github.com/open-telemetry/opentelemetry-collector/receiver"

	"github.com/open-telemetry/opentelemetry-collector-contrib/exporter/stackdriverexporter"
        "github.com/googlecloudplatform/appengine-sidecars-docker/opencensus-monitoring/receiver/filereceiver"
)

func components() (config.Factories, error) {
	errs := []error{}
	factories, err := defaults.Components()
	if err != nil {
		return config.Factories{}, err
	}

	receivers := []receiver.Factory{
		&filereceiver.Factory{},
	}
	for _, rcv := range factories.Receivers {
		receivers = append(receivers, rcv)
	}
	factories.Receivers, err = receiver.Build(receivers...)
	if err != nil {
		errs = append(errs, err)
	}

	exporters := []exporter.Factory{&stackdriverexporter.Factory{}}
	for _, exp := range factories.Exporters {
		exporters = append(exporters, exp)
	}
	factories.Exporters, err = exporter.Build(exporters...)
	if err != nil {
		errs = append(errs, err)
	}

	return factories, oterr.CombineErrors(errs)
}
