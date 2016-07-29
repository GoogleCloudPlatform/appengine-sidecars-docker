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

# run_docker.bash runs all tests in a Docker container.

if [ $(basename $PWD) != appengine-sidecars-docker ]; then
  echo "This script should be run from the appengine-sidecars-docker directory."
  exit 1
fi

set -e -x

SRC=/gopath/src/github.com/GoogleCloudPlatform/appengine-sidecars-docker

docker run --rm -i \
  -e GOPATH=/gopath \
  -w $SRC \
  -v $PWD:$SRC \
  -t quay.io/travisci/travis-ruby:latest \
  bash -c 'testing/docker_install_deps.bash && PATH=$PATH:/usr/local/go/bin testing/all.bash'
