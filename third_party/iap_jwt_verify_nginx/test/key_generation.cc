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

#include "test/key_generation.h"

#include <fstream>

#include "openssl/bn.h"
#include "openssl/obj_mac.h"
#include "test/b64_encode.h"

namespace google {
namespace cloud {
namespace iap {
namespace test {

using std::string;

string bignum_to_b64(const BIGNUM *bn) {
  const unsigned num_bytes = BN_num_bytes(bn);
  uint8_t *bytes = new uint8_t[num_bytes];
  size_t written = BN_bn2bin(bn, bytes);
  string b64 = test::url_safe_b64_encode(bytes, written);
  delete [] bytes;
  return b64;
}

void get_pubkey_points(EC_KEY *key, BIGNUM **x, BIGNUM **y) {
  *x = BN_new();
  *y = BN_new();
  EC_POINT_get_affine_coordinates_GFp(
      EC_KEY_get0_group(key), EC_KEY_get0_public_key(key), *x, *y, NULL);
}

void get_pubkey_points(EC_KEY *key, BIGNUM *x, BIGNUM *y) {
  EC_POINT_get_affine_coordinates_GFp(
      EC_KEY_get0_group(key), EC_KEY_get0_public_key(key), x, y, NULL);
}

bssl::UniquePtr<EC_KEY> gen_P256_key() {
  bssl::UniquePtr<EC_KEY> key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  if (key == nullptr) {
    return nullptr;
  }

  if (EC_KEY_generate_key(key.get()) == 0) {
    return nullptr;
  }

  return key;
}

Json::Value ec_key_to_jwk(EC_KEY *key, std::string kid) {
  Json::Value jwk;
  jwk["alg"] = "ES256";
  jwk["crv"] = "P-256";
  jwk["kid"] = kid;
  jwk["kty"] = "EC";
  jwk["use"] = "sig";
  BIGNUM *x, *y;
  get_pubkey_points(key, &x, &y);
  jwk["x"] = bignum_to_b64(x);
  jwk["y"] = bignum_to_b64(y);
  BN_free(x);
  BN_free(y);
  return jwk;
}

void create_key_files(std::string pub_key_fname, std::string priv_key_fname) {
  std::string kids[] { "1a", "2b", "3c" };
  Json::Value jwks;
  Json::Value private_keys;
  for (size_t i = 0; i < 3; i++) {
    bssl::UniquePtr<EC_KEY> key(gen_P256_key());
    jwks["keys"].append(ec_key_to_jwk(key.get(), kids[i]));
    private_keys[kids[i]] = bignum_to_b64(EC_KEY_get0_private_key(key.get()));
  }
  std::fstream public_key_file(pub_key_fname, std::ios::out|std::ios::trunc);
  std::fstream private_key_file(priv_key_fname, std::ios::out|std::ios::trunc);
  public_key_file << jwks ;
  private_key_file << private_keys ;
  return;
}

Json::Value gen_jwk(std::string kid) {
  bssl::UniquePtr<EC_KEY> key(gen_P256_key());
  return ec_key_to_jwk(key.get(), kid);
}

Json::Value gen_jwk() {
  return gen_jwk("N2_6BA");
}

}  // namespace test
}  // namespace iap
}  // namespace cloud
}  // namespace google
