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

#include "third_party/iap_jwt_verify_nginx/src/key_util.h"

#include "openssl/bn.h"
#include "openssl/ec_key.h"
#include "openssl/obj_mac.h"
#include "third_party/iap_jwt_verify_nginx/src/base64_util.h"
#include "third_party/iap_jwt_verify_nginx/src/jwt.h"

namespace google {
namespace cloud {
namespace iap {

std::shared_ptr<iap_key_map_t> process_keys(const Json::Value &keys) {
  if (!keys.isArray() || keys.empty()) {
    return nullptr;
  }

  std::shared_ptr<iap_key_map_t> key_map = std::make_shared<iap_key_map_t>();
  unsigned int num_keys = keys.size();
  for (unsigned int i = 0; i < num_keys; i++) {
    Json::Value jwk = keys[i];
    if (jwk.empty()) {
      continue;
    }

    // Re-use extract_string from JWT parsing.
    std::string kid;
    if (!Jwt::extract_string("kid", jwk, &kid)) {
      continue;
    }

    bssl::UniquePtr<EC_KEY> key = ec_key_from_jwk(jwk);
    if (key == nullptr) {
      continue;
    }

    (*key_map)[kid] = std::move(key);
  }

  return key_map;
}

bssl::UniquePtr<EC_KEY> ec_key_from_jwk(const Json::Value &jwk) {
  bssl::UniquePtr<BIGNUM> x = extract_bignum(jwk, "x");
  if (x == nullptr) {
    return nullptr;
  }

  bssl::UniquePtr<BIGNUM> y = extract_bignum(jwk, "y");
  if (y == nullptr) {
    return nullptr;
  }

  // At this point, we have valid BIGNUMs for x and y, so go ahead and
  // allocate an EC_KEY.
  EC_KEY *key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  if (key == NULL) {
    return nullptr;
  }

  if (EC_KEY_set_public_key_affine_coordinates(key, x.get(), y.get()) == 0) {
    EC_KEY_free(key);
    return nullptr;
  }

  return bssl::UniquePtr<EC_KEY>(key);
}

bssl::UniquePtr<BIGNUM> extract_bignum(const Json::Value &jwk,
                                       const std::string &key) {
  std::string val;
  if (!Jwt::extract_string(key, jwk, &val)) {
    return nullptr;
  }

  return b64_to_bignum(val);
}

bssl::UniquePtr<BIGNUM> b64_to_bignum(const std::string &b64) {
  size_t len;
  std::unique_ptr<const uint8_t[]> bytes = url_safe_base64_decode(b64, &len);
  if (bytes == nullptr) {
    return nullptr;
  }

  // Functions from OpenSSL are pure C, so we use NULL when dealing with them.
  BIGNUM *bn = BN_bin2bn(bytes.get(), len, NULL);
  if (bn == NULL) {
    return nullptr;
  }

  return bssl::UniquePtr<BIGNUM>(bn);
}

}  // namespace iap
}  // namespace cloud
}  // namespace google

