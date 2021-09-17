// Copyright 2019 OpenTelemetry Authors
// Modifications Copyright 2020 Google Inc. All rights reserved.
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

// Program otelcontribcol is the Omnition Telemetry Service built on top of
// OpenTelemetry Service.
package main

import (
	"fmt"
	"log"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/service"
)

func main() {
	handleErr := func(err error) {
		if err != nil {
			log.Fatalf("Failed to run the service: %v", err)
		}
	}
	factories, err := components()
	handleErr(err)

	app, err := service.New(service.CollectorSettings{
		Factories: factories,
		BuildInfo: component.BuildInfo{
			Command:     "otelcontribcol",
			Description: "AppEngine Flex OpenTelemetry Contrib Collector",
			Version:     "latest",
		},
	})

	if err != nil {
		handleErr(fmt.Errorf("failed to construct the application: %w", err))
	}

	if app.Run() != nil {
		handleErr(fmt.Errorf("application run finished with error: %w", err))
	}
}
