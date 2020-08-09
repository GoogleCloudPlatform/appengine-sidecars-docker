# Copyright (C) 2002-2016 Igor Sysoev
# Copyright (C) 2011-2016 Nginx, Inc.
# Copyright (C) 2020 Google Inc.
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
########################################

licenses(["notice"])  #BSD license

cc_library(
    name = "ngx_latency_status_module",
    srcs = [
        "ngx_http_latency_stub_status_module.c",
        "ngx_http_latency_stub_status_module.h",
        ],
    linkstatic = 1,
    visibility = [
        "//visibility:public",
    ],
    defines = [
        "NGX_LATENCY_STATUS",
    ],
    deps = [
        ":ngx_latency_status_lib",
        "@nginx//:http",
    ],
)

cc_library(
    name = "ngx_latency_status_lib",
    srcs = [
        "ngx_http_latency_recorder.c",
        "ngx_http_latency_status_handler.c",
        "ngx_http_latency_storage.c",
    ],
    hdrs = [
        "ngx_http_latency_recorder.h",
        "ngx_http_latency_status_handler.h",
        "ngx_http_latency_storage.h"
    ],
    deps = [
        "@nginx//:http"
    ]
)
