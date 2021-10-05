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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

_JSONCPP_BUILD_FILE = """
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
# Used to build the JsonCpp library.

licenses(["notice"])

cc_library(
    name = "lib_json",
    srcs = glob(
        ["jsoncpp-1.8.0/src/lib_json/*.cpp"],
    ),
    hdrs = glob([
        "jsoncpp-1.8.0/include/json/*.h",
        "jsoncpp-1.8.0/src/lib_json/*.h",
        "jsoncpp-1.8.0/src/lib_json/*.inl",
    ]),
    copts = [
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/include",
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/src/lib_json",
    ],
    visibility = ["//visibility:public"],
)
"""

def appengine_nginx_repositories(have_nginx):
    if (not have_nginx):
        git_repository(
            name = "nginx",
            commit = "2bb5830cb3512a85909d92f0eba6313a231bb15d", #  Jul 08, 2021
            remote = "https://nginx.googlesource.com/nginx",
            shallow_since = "1626739331 +0000",
        )

    http_archive(
        name = "jsoncpp",
        url = "https://github.com/open-source-parsers/jsoncpp/archive/1.8.0.zip",
        sha256 = "4dd616d24ce537dfbc22b4dd81bf6ff8d80577a6bbb47cda9afb8445e4661f9b",
        build_file_content = _JSONCPP_BUILD_FILE,
    )
