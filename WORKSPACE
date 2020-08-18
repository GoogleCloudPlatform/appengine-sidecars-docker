# Copyright 2017 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###############################################################################
#
# A Bazel workspace for appengine's nginx modules.

workspace(name = "appengine_nginx")

load(":appengine_nginx.bzl", "appengine_nginx_repositories")

appengine_nginx_repositories(False)

load("@nginx//:build.bzl", "nginx_repositories")

nginx_repositories(
    bind = True,
    nginx = "@nginx//",
)

new_http_archive(
    name = "gtest",
    url = "https://github.com/google/googletest/archive/release-1.8.0.zip",
    sha256 = "f3ed3b58511efd272eb074a3a6d6fb79d7c2e6a0e374323d1e6bcbcc1ef141bf",
    build_file = "third_party/iap_jwt_verify_nginx/gtest.BUILD",
)
