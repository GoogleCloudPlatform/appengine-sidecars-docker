// main launches the memcache d/g proxy bridge with a context suitable
// for managed VMs.
package main

import (
	"flag"
	"log"
	"os"

	"dtog"
	gaeint "google.golang.org/appengine/notreallyinternal"
)

var (
	bindingAddr = flag.String("binding_address", "localhost:11211",
		"host:port the memcached daemon will listen on. For example 0.0.0.0:11211.")
	application = flag.String("application", "", "app ID override")
	module      = flag.String("module", "", "module override")
	version     = flag.String("version", "", "major version override")
	instance    = flag.String("instance", "", "instance override")
	apiHost     = flag.String("api_host", "appengine.googleapis.internal", "for dev, set to the 'localhost'")
	apiPort     = flag.String("api_port", "10001",
		"for dev, set to the <port>' as reported by dev_appserver: 'Starting API "+
			"server at: http://localhost:<port>'")
)

func init() {
	log.SetFlags(log.Lshortfile | log.LstdFlags)
	log.SetPrefix("Memcache-proxy: ")
}

func main() {
	flag.Parse()

	// Apply the overrides.
	// The final order of overrides will be:
	// 1. those cmd line parameters
	// 2. the matching standard GAE env variables (see below)
	// 3. the VM metadata entries ("instance/attributes/gae_project" for example.)
	if application != nil {
		if err := os.Setenv("GAE_LONG_APP_ID", *application); err != nil {
			panic(err)
		}
	}
	if module != nil {
		if err := os.Setenv("GAE_MODULE_NAME", *module); err != nil {
			panic(err)
		}
	}
	if version != nil {
		if err := os.Setenv("GAE_MODULE_VERSION", *version); err != nil {
			panic(err)
		}
	}
	if instance != nil {
		if err := os.Setenv("GAE_MODULE_INSTANCE", *instance); err != nil {
			panic(err)
		}
	}
	if apiHost != nil {
		if err := os.Setenv("API_HOST", *apiHost); err != nil {
			panic(err)
		}
	}
	if apiPort != nil {
		if err := os.Setenv("API_PORT", *apiPort); err != nil {
			panic(err)
		}
	}
	ctx := gaeint.BackgroundContext()
	err := dtog.StartSync(ctx, *bindingAddr) // should never return
	log.Fatalf("problem with d/g proxy %v", err)
}
