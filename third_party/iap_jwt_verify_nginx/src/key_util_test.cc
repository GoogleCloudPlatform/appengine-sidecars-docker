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

#include "key_util.h"

#include <sstream>

#include "openssl/bn.h"
#include "gtest/gtest.h"
#include "test/b64_encode.h"
#include "test/key_generation.h"

namespace google {
namespace cloud {
namespace iap {
namespace {

using std::string;

////////////////////////////////////////////////////////////////////////////////
// process_keys tests
TEST(KeyUtilTest, ProcessKeysSuccess) {
  string kids[] {
    "abcdef",
    "123456",
    "BANANA",
  };
  string x_vals[] {
    "YlSGOnV5xFM6O6hJ7Cw8_AglyhclCRYyaequoF3wJE0",
    "XhpkM377rmSHSxtCDA4cm8k-fbcHrB8ocopAsF_E0bw",
    "DeWskyWGUC3avt4cwc2TFFosEXD126nqXTD3eJn2P0s",
  };
  string y_vals[] {
    "wzwR-TzfJs6_MgmzJbiEab13FCBL3qvBctOOWW_4UQE",
    "NUw4sVLhi2hvENJP6tBy2VmvGpJc6KlTBxt4U9sjwzI",
    "uv1dIkl3FC5TzsmgBH3BOgWEPUFJ5spSMzhaFhBy-_s",
  };
  std::stringstream ss("");
  ss << "[";
  for (size_t i = 0; i < 3; i++) {
    ss << R"({"alg" : "ES256", "crv" : "P-256", "kid" : ")"
       << kids[i]
       << R"(", "kty" : "EC", "use" : "sig", "x" : ")"
       << x_vals[i]
       << R"(", "y" : ")"
       << y_vals[i]
       << R"("})";
    if (i != 2) {
      ss << ", ";
    } else {
      ss << "]";
    }
  }
  Json::Value keys;
  ss >> keys;
  std::shared_ptr<iap_key_map_t> key_map = process_keys(keys);
  EXPECT_TRUE(key_map != nullptr);
  EXPECT_EQ(3, key_map->size());
  BIGNUM *x = BN_new();
  BIGNUM *y = BN_new();
  for (size_t i = 0; i < 3; i++) {
    bssl::UniquePtr<EC_KEY> &key = (*key_map)[kids[i]];
    EXPECT_TRUE(key != nullptr);
    test::get_pubkey_points(key.get(), x, y);
    EXPECT_EQ(x_vals[i], test::bignum_to_b64(x));
    EXPECT_EQ(y_vals[i], test::bignum_to_b64(y));
  }
  BN_free(x);
  BN_free(y);
}

TEST(KeyUtilTest, ProcessKeysNoKeys) {
  Json::Value keys;
  EXPECT_EQ(nullptr, process_keys(keys));
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ec_key_from_jwk tests
TEST(KeyUtilTest, EcKeyFromJwkSuccess) {
  std::stringstream jwk_stream(R"({
      "alg" : "ES256",
      "crv" : "P-256",
      "kid" : "N2_6BA",
      "kty" : "EC",
      "use" : "sig",
      "x" : "EWujU1VF8-7eEwkJakVe0qmXjE9j46V10i1MPEBXkwY",
      "y" : "gdtVUVQMtE-4Lhf6OpKMb3o0iRAzPoVlNFn-NrJGG7A"})");
  Json::Value jwk;
  jwk_stream >> jwk;
  bssl::UniquePtr<EC_KEY> key = ec_key_from_jwk(jwk);
  EXPECT_FALSE(key == nullptr);
  BIGNUM *x, *y;
  test::get_pubkey_points(key.get(), &x, &y);
  EXPECT_EQ("EWujU1VF8-7eEwkJakVe0qmXjE9j46V10i1MPEBXkwY",
            test::bignum_to_b64(x));
  EXPECT_EQ("gdtVUVQMtE-4Lhf6OpKMb3o0iRAzPoVlNFn-NrJGG7A",
            test::bignum_to_b64(y));
  BN_free(x);
  BN_free(y);
}

TEST(KeyUtilTest, EcKeyFromJwkNoX) {
  Json::Value jwk;
  jwk["y"] = "2JQh8G8IEY1QoaixCWTaFpZksA";
  EXPECT_EQ(nullptr, ec_key_from_jwk(jwk));
}

TEST(KeyUtilTest, EcKeyFromJwkNoY) {
  Json::Value jwk;
  jwk["x"] = "2JQh8G8IEY1QoaixCWTaFpZksA";
  EXPECT_EQ(nullptr, ec_key_from_jwk(jwk));
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// extract_bignum tests
TEST(KeyUtilTest, ExtractBignumSuccess) {
  Json::Value jwk;
  jwk["x"] = "2JQh8G8IEY1QoaixCWTaFpZksA";
  bssl::UniquePtr<BIGNUM> x = extract_bignum(jwk, "x");
  EXPECT_TRUE(x != nullptr);
  BIGNUM *expected = nullptr;
  BN_dec2bn(&expected, "4829865130109851095612340958110586123324908720");
  EXPECT_EQ(0, BN_cmp(expected, x.get()));
  BN_free(expected);
}

TEST(KeyUtilTest, ExtractBignumKeyNotPresent) {
  Json::Value jwk;
  jwk["z"] = "ABCD";
  EXPECT_EQ(nullptr, extract_bignum(jwk, "x"));
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// b64_to_bignum tests
TEST(KeyUtilTest, B64ToBignumSuccess) {
  string b64("2JQh8G8IEY1QoaixCWTaFpZksA");
  bssl::UniquePtr<BIGNUM> from_b64 = b64_to_bignum(b64);
  EXPECT_TRUE(from_b64 != nullptr);
  BIGNUM *expected = nullptr;
  BN_dec2bn(&expected, "4829865130109851095612340958110586123324908720");
  EXPECT_EQ(0, BN_cmp(expected, from_b64.get()));
  BN_free(expected);
}

TEST(KeyUtilTest, B64ToBignumBadB64) {
  string b64("yummy");
  EXPECT_TRUE(b64_to_bignum(b64) == nullptr);
}
////////////////////////////////////////////////////////////////////////////////

}  // namespace
}  // namespace iap
}  // namespace cloud
}  // namespace google
