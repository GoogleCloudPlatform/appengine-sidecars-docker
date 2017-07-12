# Copyright (C) 2002-2016 Igor Sysoev
# Copyright (C) 2011-2016 Nginx, Inc.
# Copyright (C) 2017 Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
################################################################################

py_test(
    name = "nginx_test",
    srcs = ["nginx_test.py"],
    data = [
        ":nginx-iap",
        ":keys",
    ],
    default_python_version = "PY2",
)

# Full nginx binary with module compiled in for testing
cc_binary(
    name = "nginx-iap",
    srcs = [
        "nginx_modules.c",
    ],
    linkstatic = 1,
    deps = [
        "//src:ngx_iap_jwt_verify",
        "@nginx//:http",
        "@nginx//:http_access",
        "@nginx//:http_addition",
        "@nginx//:http_auth_request",
        "@nginx//:http_autoindex",
        "@nginx//:http_browser",
        "@nginx//:http_charset",
        "@nginx//:http_geo",
        "@nginx//:http_gunzip",
        "@nginx//:http_gzip_filter",
        "@nginx//:http_gzip_static",
        "@nginx//:http_limit_conn",
        "@nginx//:http_limit_req",
        "@nginx//:http_map",
        "@nginx//:http_proxy",
        "@nginx//:http_realip",
        "@nginx//:http_referer",
        "@nginx//:http_rewrite",
        "@nginx//:http_split_clients",
        "@nginx//:http_stub_status",
        "@nginx//:http_sub",
        "@nginx//:http_upstream_hash",
        "@nginx//:http_upstream_ip_hash",
        "@nginx//:http_upstream_keepalive",
        "@nginx//:http_upstream_least_conn",
        "@nginx//:http_userid",
        "@nginx//:stream",
        "@nginx//:stream_access",
        "@nginx//:stream_geo",
        "@nginx//:stream_limit_conn",
        "@nginx//:stream_map",
        "@nginx//:stream_return",
        "@nginx//:stream_split_clients",
        "@nginx//:stream_upstream_hash",
        "@nginx//:stream_upstream_least_conn",
    ],
)

# Utility binary to generate JWTs for testing
cc_binary(
    name = "gen_iap_jwt",
    testonly = 1,
    srcs = [
        "jwt_generator.cc",
    ],
    deps = [
        ":iap_jwt_generation",
        "@jsoncpp//:lib_json",
    ],
    data = [
        ":keys",
    ],
    copts = [
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/include",
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/src/lib_json",
    ],
)

# Utility binary to create public/private key files for use in testing
cc_binary(
    name = "gen_keys",
    testonly = 1,
    srcs = [
        "key_generator.cc",
    ],
    deps = [
        ":iap_jwt_generation",
        "@boringssl//:crypto",
        "@jsoncpp//:lib_json",
    ],
    copts = [
        "-Iexternal/boringssl/include",
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/include",
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/src/lib_json",
    ],
)

cc_library(
    name = "iap_jwt_generation",
    testonly = 1,
    srcs = [
        "b64_encode.cc",
        "jwt_generation.cc",
        "key_generation.cc",
    ],
    hdrs = [
        "b64_encode.h",
        "jwt_generation.h",
        "key_generation.h",
    ],
    deps = [
        "//src:iap_jwt_verification",
        "@boringssl//:crypto",
        "@jsoncpp//:lib_json",
    ],
    copts = [
        "-Iexternal/boringssl/include",
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/include",
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/src/lib_json",
    ],
    visibility = [
        "//src:__pkg__",
    ],
)

filegroup(
    name = "keys",
    srcs = [
        "keys.jwk",
        "keys.priv",
    ],
    visibility = [
        "//src:__pkg__",
    ],
)
