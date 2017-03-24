#!/bin/bash
# Copyright 2015 Google Inc. All rights reserved.
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

set -e

echo "Preparing build ..."
export GOPATH=$(mktemp --directory)
mkdir -p $GOPATH/src $GOPATH/pkg

DEST=$GOPATH/src/github.com/GoogleCloudPlatform/cloudsql-proxy
git clone https://github.com/GoogleCloudPlatform/cloudsql-proxy $DEST
# Pin the version of the proxy. Bump this to get a new version.
git -C $DEST checkout 2e269df091b60330e96d48f857a652ff13ed2994
go get -d github.com/GoogleCloudPlatform/cloudsql-proxy/...
echo "Building in $DEST"

CGO_ENABLED=0 GOOS=linux go build -a -installsuffix cgo -o cloud_sql_proxy github.com/GoogleCloudPlatform/cloudsql-proxy/cmd/cloud_sql_proxy/

echo "Done, your binary is here ./cloud_sql_proxy"
