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

# docker_install_deps.bash installs all dependencies, assuming it is running on a Travis machine. (e.g., quay.io/travisci/travis-ruby)

set -e -x

####### Install Go

mkdir -p /tmp
pushd /tmp

GOROOT="${GOROOT:-/usr/local/go}"
GO_VERSION="${GO_VERSION:-1.6.3}"
GO_ARCHIVE="https://storage.googleapis.com/golang/go${GO_VERSION}.linux-amd64.tar.gz"
GO_ARCHIVE_SHA="${GO_ARCHIVE}.sha256"

curl -fsSL "$GO_ARCHIVE" -o go.tar.gz
curl -fsSL "$GO_ARCHIVE_SHA" -o go.tar.gz.sha256

echo "$(cat go.tar.gz.sha256)  go.tar.gz" | sha256sum -c -

mkdir -p $GOROOT
tar -C $GOROOT --strip-components=1 -xzf go.tar.gz

popd

####### Install pip and google_compute_engine
apt-get update && apt-get install -y python-pip
pip install google-compute-engine
