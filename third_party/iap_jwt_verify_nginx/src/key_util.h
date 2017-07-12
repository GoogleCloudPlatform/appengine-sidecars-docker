// Copyright (C) 2002-2016 Igor Sysoev
// Copyright (C) 2011-2016 Nginx, Inc.
// Copyright (C) 2017 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
////////////////////////////////////////////////////////////////////////////////

#ifndef NGINX_IAP_JWT_VERIFY_KEY_UTIL_H
#define NGINX_IAP_JWT_VERIFY_KEY_UTIL_H

#include "json/json.h"
#include "src/types.h"

namespace google {
namespace cloud {
namespace iap {

// Extract the curve P-256 public keys contained in |keys|, the array of keys
// extracted from a JSON web key object. The returned shared_ptr will point to
// nullptr if an error is encountered.
std::shared_ptr<iap_key_map_t> process_keys(const Json::Value &keys);

// Extracts an EC_KEY from the provided jwk.
bssl::UniquePtr<EC_KEY> ec_key_from_jwk(const Json::Value &jwk);

// Extract a bignum stored in the value that |key| is mapped to.
bssl::UniquePtr<BIGNUM> extract_bignum(const Json::Value &jwk,
                                       const std::string &key);

// Attempt to convert the provided string to a bignum. It assumes the string is
// a url-safe base 64 encoding of a big-endian number. The returned unique_ptr
// will point to nullptr if there was an error.
bssl::UniquePtr<BIGNUM> b64_to_bignum(const std::string &b64);

}  // namespace iap
}  // namespace cloud
}  // namespace google

#endif  // NGINX_IAP_JWT_VERIFY_KEY_UTIL_H

