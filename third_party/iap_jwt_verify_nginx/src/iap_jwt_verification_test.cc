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

#include "third_party/iap_jwt_verify_nginx/src/iap_jwt_verification.h"

#include <string>

#include "gtest/gtest.h"

namespace google {
namespace cloud {
namespace iap {
namespace {

// alg: ES256
// typ: JWT
// kid: 1a
// iat: 1001
// exp: 1600
// aud: /projects/1234/apps/some-app-id
// email: nobody@example.com
// iss: https://cloud.google.com/iap
// sub: accounts.google.com:12345
constexpr char VALID_JWT[] = "eyJhbGciOiJFUzI1NiIsImtpZCI6IjFhIiwidHlwIjoiSldUIn0.eyJhdWQiOiIvcHJvamVjdHMvMTIzNC9hcHBzL3NvbWUtYXBwLWlkIiwiZW1haWwiOiJub2JvZHlAZXhhbXBsZS5jb20iLCJleHAiOjE2MDAsImlhdCI6MTAwMSwiaXNzIjoiaHR0cHM6Ly9jbG91ZC5nb29nbGUuY29tL2lhcCIsInN1YiI6ImFjY291bnRzLmdvb2dsZS5jb206MTIzNDUifQ.Oez6MwXTCLuJ-U8VHVgiHtADGv818ZMQjnjOQSgUJ9NU1UcLT5ZyT8DqgbAnRLuv0Tf9hdmcBilouJD46oPo2w";
const size_t VALID_JWT_LEN = strlen(VALID_JWT);

// A time at which VALID_JWT should be valid.
constexpr uint64_t VALID_TIME = 1100;

constexpr char AUD[] = "/projects/1234/apps/some-app-id";
const size_t AUD_LEN = strlen(AUD);

constexpr char KEY_FILE_NAME[] = "third_party/iap_jwt_verify_nginx/test/keys.jwk";

std::shared_ptr<iap_key_map_t> get_keys() {
  return load_keys(KEY_FILE_NAME, strlen(KEY_FILE_NAME));
}

// This test ensures that a valid JWT passes the validity check.
TEST(IapJwtVerificationTest, ValidJwtIsValid) {
  bool is_valid = iap_jwt_is_valid(
      VALID_JWT, VALID_JWT_LEN, VALID_TIME, AUD, AUD_LEN, get_keys());
  EXPECT_TRUE(is_valid);
}

// Ensures that a null raw JWT is invalid.
TEST(IapJwtVerificationTest, NullRawJwtIsInvalid) {
  bool is_valid = iap_jwt_is_valid(
      nullptr, VALID_JWT_LEN, VALID_TIME, AUD, AUD_LEN, get_keys());
  EXPECT_FALSE(is_valid);
}

// Ensures that JWTs exceeding a certain size are invalid.
TEST(IapJwtVerificationTest, TooLargeJwtIsInvalid) {
  std::string big_jwt(4097, 'B');  // "B" is for "BIG"
  bool is_valid = iap_jwt_is_valid(
      big_jwt.data(), big_jwt.length(), VALID_TIME, AUD, AUD_LEN, get_keys());
  EXPECT_FALSE(is_valid);
}

// Ensures that if the JWT doesn't parse (i.e. a valid Jwt object cannot be
// constructed), the JWT is considered invalid.
TEST(IapJwtVerificationTest, UnparseableJwtIsInvalid) {
  std::string bad_jwt("unparseable jwt");
  bool is_valid = iap_jwt_is_valid(
      bad_jwt.data(), bad_jwt.length(), VALID_TIME, AUD, AUD_LEN, get_keys());
  EXPECT_FALSE(is_valid);
}

// Ensures that a JWT minted 60 seconds in the future is valid, but a JWT minted
// 61 seconds in the future is invalid.
TEST(IapJwtVerificationTest, IatClockSkew) {
  bool is_valid = iap_jwt_is_valid(
      VALID_JWT, VALID_JWT_LEN, 941, AUD, AUD_LEN, get_keys());
  EXPECT_TRUE(is_valid);
  is_valid = iap_jwt_is_valid(
      VALID_JWT, VALID_JWT_LEN, 940, AUD, AUD_LEN, get_keys());
  EXPECT_FALSE(is_valid);
}

// Ensures that a JWT that expired 60 seconds ago is valid, but a JWT that
// expired 61 seconds ago is invalid.
TEST(IapJwtVerificationTest, ExpClockSkew) {
  bool is_valid = iap_jwt_is_valid(
      VALID_JWT, VALID_JWT_LEN, 1660, AUD, AUD_LEN, get_keys());
  EXPECT_TRUE(is_valid);
  is_valid = iap_jwt_is_valid(
      VALID_JWT, VALID_JWT_LEN, 1661, AUD, AUD_LEN, get_keys());
  EXPECT_FALSE(is_valid);
}

// Ensures that an audience mismatch results in a verification failure.
TEST(IapJwtVerificationTest, AudMismatchIsInvalid) {
  std::string wrong_aud("wrong");
  bool is_valid = iap_jwt_is_valid(
      VALID_JWT, VALID_JWT_LEN, VALID_TIME, wrong_aud.data(),
      wrong_aud.length(), get_keys());
  EXPECT_FALSE(is_valid);
}

// Ensures that if the shared pointer passed for the key map somehow points to
// nullptr, the JWT is invalid and nothing drastic (e.g. segfault) occurs.
TEST(IapJwtVerificationTest, NullKeysIsInvalid) {
  bool is_valid = iap_jwt_is_valid(
      VALID_JWT, VALID_JWT_LEN, VALID_TIME, AUD, AUD_LEN, nullptr);
  EXPECT_FALSE(is_valid);
}

// Ensures that a JWT with an invalid signature is considered invalid.
TEST(IapJwtVerificationTest, InvalidSignatureIsInvalid) {
  std::vector<std::string> jwt_parts;
  Jwt::split_jwt(std::string(VALID_JWT), &jwt_parts);
  std::string bad_sig_jwt = jwt_parts[0] + "." + jwt_parts[1] + "." + "badsig";
  bool is_valid = iap_jwt_is_valid(
      bad_sig_jwt.data(), bad_sig_jwt.length(), VALID_TIME, AUD, AUD_LEN,
      get_keys());
  EXPECT_FALSE(is_valid);
}

}  // namespace
}  // namespace iap
}  // namespace cloud
}  // namespace google
