/*
Copyright 2015 Google Inc. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

// Command memcachep runs a memcache d/g proxy bridge for App Engine flexible environment.
package main

import (
	"flag"
	"log"
	"os"

	"dtog"

	gaeint "google.golang.org/appengine/notreallyinternal"
)

const (
	applicationEnv = "GAE_LONG_APP_ID"
	moduleEnv      = "GAE_MODULE_NAME"
	versionEnv     = "GAE_MODULE_VERSION"
	instanceEnv    = "GAE_MODULE_INSTANCE"

	apiHostEnv = "API_HOST"
	apiPortEnv = "API_PORT"
)

var (
	addr = flag.String("binding_address", "localhost:11211", "Address to listen on.")

	application = flag.String("application", os.Getenv(applicationEnv), "The GAE App ID.")
	module      = flag.String("module", os.Getenv(moduleEnv), "The GAE Module ID.")
	version     = flag.String("version", os.Getenv(versionEnv), "The GAE major version.")
	instance    = flag.String("instance", os.Getenv(instanceEnv), "The GAE instance ID.")

	apiHost = flag.String("api_host", getenvDefault(apiHostEnv, "appengine.googleapis.internal"),
		"Host of the GAE API Server. For dev_appserver, use localhost.")
	apiPort = flag.String("api_port", getenvDefault(apiPortEnv, "10001"),
		"Port of the GAE API Server. For dev_appserver, use the reported when starting dev_appserver.")
)

func init() {
	log.SetFlags(log.Lshortfile | log.LstdFlags)
	log.SetPrefix("Memcache-proxy: ")
}

func main() {
	flag.Parse()

	// Apply environment variable overrides, in order of preference:
	// 1. Command-line flags.
	// 2. Matching standard GAE env variables (see below)
	// 3. VM metadata entries (e.g., "instance/attributes/gae_project")
	mustSetenv(applicationEnv, *application)
	mustSetenv(moduleEnv, *module)
	mustSetenv(versionEnv, *version)
	mustSetenv(instanceEnv, *instance)
	mustSetenv(apiHostEnv, *apiHost)
	mustSetenv(apiPortEnv, *apiPort)

	ctx := gaeint.BackgroundContext()
	log.Fatalf("StartSync: %v", dtog.StartSync(ctx, *addr))
}

// getenvDefault returns the value of os.Getenv(k), or d if the environment variable does not exist.
func getenvDefault(k, d string) string {
	if v := os.Getenv(k); v != "" {
		return v
	}
	return d
}

// mustSetenv sets an environment variable.
func mustSetenv(k, v string) {
	if err := os.Setenv(k, v); err != nil {
		log.Fatalf("Setenv %s=%s: %v", k, v, err)
	}
}
