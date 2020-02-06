#!/bin/bash
# Copyright 2016 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# run_docker.bash runs all tests.

#### TODO: Add tests for:
# api_proxy
# api_verifier
# fluentd_logger
# nginx_proxy
####

pwd
if [ $(basename $PWD) != appengine-sidecars-docker ]; then
  echo "This script should be run from the appengine-sidecars-docker directory."
  exit 1
fi

set -e -x

#### Run Go tests.

which go
go env
# NOTE(cbro): skip memcache_proxy, it is not go-gettable.
GO_PKGS=$(go list -v github.com/GoogleCloudPlatform/appengine-sidecars-docker/... | grep -v memcache_proxy)
GO_PKGS_NO_MODULES=$(echo $GO_PKGS | grep -e api_proxy)
GO_PKGS_WITH_MODULES=$(echo $GO_PKGS | grep -v api_proxy)

go test -v $GO_PKGS_NO_MODULES

env GO111MODULE=on go test -v $GO_PKGS_WITH_MODULES

#### go vet, go fmt, etc.

# NOTE(cbro): vet has false positives. If a check fails, consider removing this or ignoring that check.
go vet ./...
diff -u <(echo -n) <(gofmt -d -s .)

#### Build Cloud SQL

pushd cloud_sql_proxy
./build.sh
popd

#### Build Memcache proxy

pushd memcache_proxy
./build.sh
popd

#### Run tests for iap_watcher
pushd iap_watcher
./test.sh
popd
