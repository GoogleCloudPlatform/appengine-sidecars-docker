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

gtest_include_copt = (
    "-Iexternal/gtest/googletest-release-1.8.0/googletest/include")

cc_library(
    name = "ngx_iap_jwt_verify",
    srcs = [
        "module.h",
        "module.cc",
    ],
    linkstatic = 1,
    visibility = [
        "//visibility:public",
    ],
    defines = [
        "NGX_IAP_JWT_VERIFY",
    ],
    deps = [
        ":iap_jwt_verification",
        "@boringssl//:crypto",
        "@jsoncpp//:lib_json",
        "@nginx//:http",
    ],
    copts = [
        "-Iexternal/boringssl/include",
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/include",
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/src/lib_json",
    ],
)

cc_library(
    name = "iap_jwt_verification",
    srcs = [
        "base64_util.cc",
        "iap_jwt_verification.cc",
        "jwt.cc",
        "key_util.cc",
    ],
    visibility = [
        "//test:__pkg__",
    ],
    hdrs = [
        "base64_util.h",
        "iap_jwt_verification.h",
        "jwt.h",
        "key_util.h",
        "types.h",
    ],
    deps = [
        "@boringssl//:crypto",
        "@jsoncpp//:lib_json",
    ],
    copts = [
        "-Iexternal/boringssl/include",
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/include",
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/src/lib_json",
    ],
)

cc_test(
    name = "base64_util_test",
    srcs = [
        "base64_util_test.cc",
    ],
    deps = [
        ":iap_jwt_verification",
        "@gtest//:main",
    ],
    copts = [
        gtest_include_copt,
    ],
    size = "small",
)

cc_test(
    name = "iap_jwt_verification_test",
    srcs = [
        "iap_jwt_verification_test.cc",
    ],
    data = [
        "//test:keys",
    ],
    deps = [
        ":iap_jwt_verification",
        "@gtest//:main",
    ],
    copts = [
        gtest_include_copt,
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/include",
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/src/lib_json",
    ],
    size = "small",
)

cc_test(
    name = "jwt_test",
    srcs = [
        "jwt_test.cc",
    ],
    deps = [
        ":iap_jwt_verification",
        "//test:iap_jwt_generation",
        "@gtest//:main",
    ],
    copts = [
        "-Iexternal/boringssl/include",
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/include",
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/src/lib_json",
        gtest_include_copt,
    ],
    size = "small",
)

cc_test(
    name = "key_util_test",
    srcs = [
        "key_util_test.cc",
    ],
    deps = [
        ":iap_jwt_verification",
        "//test:iap_jwt_generation",
        "@boringssl//:crypto",
        "@gtest//:main",
        "@jsoncpp//:lib_json",
    ],
    copts = [
        "-Iexternal/boringssl/include",
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/include",
        "-Iexternal/jsoncpp/jsoncpp-1.8.0/src/lib_json",
        gtest_include_copt,
    ],
    size = "small",
)
