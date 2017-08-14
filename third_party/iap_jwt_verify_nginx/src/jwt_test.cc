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

#include "third_party/iap_jwt_verify_nginx/src/jwt.h"

#include "gtest/gtest.h"
#include "third_party/iap_jwt_verify_nginx/test/b64_encode.h"

namespace google {
namespace cloud {
namespace iap {
namespace {

using ::google::cloud::iap::test::url_safe_b64_encode;

TEST(JwtTest, ValidJwtParsesSuccessfully) {
  const std::string encoded_header =
      "eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6InhkS193dyJ9";
  const std::string encoded_payload =
      "eyJpc3MiOiJodHRwczovL2Nsb3VkLmdvb2dsZS5jb20vaWFwIiwic3ViIjoiYWNjb3VudHMu"
      "Z29vZ2xlLmNvbToxMjM0NSIsImVtYWlsIjoibm9ib2R5QGV4YW1wbGUuY29tIiwiYXVkIjoi"
      "L2EvYi9jL2QiLCJleHAiOjYwMCwiaWF0IjowfQ";
  const std::string encoded_signature = "AAE";
  const std::string jwt_str =
      encoded_header + "." + encoded_payload + "." + encoded_signature;
  std::unique_ptr<Jwt> jwt = Jwt::parse_from_string(jwt_str);
  EXPECT_TRUE(jwt != nullptr);
  EXPECT_EQ(encoded_header, jwt->encoded_header());
  EXPECT_EQ(encoded_payload, jwt->encoded_payload());

  Json::Value expected_header;
  expected_header["alg"] = "ES256";
  expected_header["typ"] = "JWT";
  expected_header["kid"] = "xdK_ww";
  EXPECT_EQ(expected_header, jwt->header());

  Json::Value expected_payload;
  expected_payload["iss"] = "https://cloud.google.com/iap";
  expected_payload["sub"] = "accounts.google.com:12345";
  expected_payload["email"] = "nobody@example.com";
  expected_payload["aud"] = "/a/b/c/d";
  expected_payload["iat"] = 0;
  expected_payload["exp"] = 600;
  EXPECT_EQ(expected_payload, jwt->payload());

  EXPECT_EQ("ES256", jwt->alg());
  EXPECT_EQ("xdK_ww", jwt->kid());
  EXPECT_EQ(0, jwt->iat());
  EXPECT_EQ(600, jwt->exp());
  EXPECT_EQ("/a/b/c/d", jwt->aud());
  EXPECT_EQ(2, jwt->signature_length());
  EXPECT_EQ(0, jwt->signature_bytes()[0]);
  EXPECT_EQ(1, jwt->signature_bytes()[1]);
}

TEST(JwtTest, UnsplittableJwtDoesNotParse) {
  std::string jwt_str("no period");
  std::unique_ptr<Jwt> jwt = Jwt::parse_from_string(jwt_str);
  EXPECT_EQ(nullptr, jwt);
}

////////////////////////////////////////////////////////////////////////////////
// Jwt::split_jwt tests
// "a.b.c" is valid
TEST(JwtTest, SplitJwtValidCase1) {
  std::string jwt("a.b.c");
  std::vector<std::string> parts;
  EXPECT_TRUE(Jwt::split_jwt(jwt, &parts));
  EXPECT_EQ(3, parts.size());
  EXPECT_EQ("a", parts[0]);
  EXPECT_EQ("b", parts[1]);
  EXPECT_EQ("c", parts[2]);
}

// "Aardvark.Banana.Catastrophe" is valid
TEST(JwtTest, SplitJwtValidCase2) {
  std::string jwt("Aardvark.Banana.Catastrophe");
  std::vector<std::string> parts;
  EXPECT_TRUE(Jwt::split_jwt(jwt, &parts));
  EXPECT_EQ(3, parts.size());
  EXPECT_EQ("Aardvark", parts[0]);
  EXPECT_EQ("Banana", parts[1]);
  EXPECT_EQ("Catastrophe", parts[2]);
}

// Another valid case.
TEST(JwtTest, SplitJwtValidCase3) {
  std::string jwt("eyJgbGeiOiJFUzI1.eyJpc3MiOiJodHRw.Std7KMOOcY5hx2nu");
  std::vector<std::string> parts;
  EXPECT_TRUE(Jwt::split_jwt(jwt, &parts));
  EXPECT_EQ(parts.size(), 3);
  EXPECT_EQ("eyJgbGeiOiJFUzI1", parts[0]);
  EXPECT_EQ("eyJpc3MiOiJodHRw", parts[1]);
  EXPECT_EQ("Std7KMOOcY5hx2nu", parts[2]);
}

// Empty string is invalid.
TEST(JwtTest, SplitJwtEmptyStringInvalid) {
  std::string jwt("");
  std::vector<std::string> parts;
  EXPECT_FALSE(Jwt::split_jwt(jwt, &parts));
}

// A JWT with no periods is invalid.
TEST(JwtTest, SplitJwtNoPeriodsInvalid) {
  std::string jwt("abc");
  std::vector<std::string> parts;
  EXPECT_FALSE(Jwt::split_jwt(jwt, &parts));
}

// A JWT with only one period is invalid.
TEST(JwtTest, SplitJwtOnePeriodInvalid) {
  std::string jwt("a.bc");
  std::vector<std::string> parts;
  EXPECT_FALSE(Jwt::split_jwt(jwt, &parts));
}

// Starting with a period is also unacceptable.
TEST(JwtTest, SplitJwtInitialPeriodInvalid) {
  std::string jwt(".a.bc");
  std::vector<std::string> parts;
  EXPECT_FALSE(Jwt::split_jwt(jwt, &parts));
}

// Ending with a period is also illegal.
TEST(JwtTest, SplitJwtEndingPeriodInvalid) {
  std::string jwt("a.bc.");
  std::vector<std::string> parts;
  EXPECT_FALSE(Jwt::split_jwt(jwt, &parts));
}

// Two adjacent periods should be invalid.
TEST(JwtTest, SplitJwtAdjacentPeriodsInvalid) {
  std::string jwt("a..b");
  std::vector<std::string> parts;
  EXPECT_FALSE(Jwt::split_jwt(jwt, &parts));
}

// Three periods should be invalid.
TEST(JwtTest, SplitJwtThreePeriodsInvalid) {
  std::string jwt("a.b.c.d");
  std::vector<std::string> parts;
  EXPECT_FALSE(Jwt::split_jwt(jwt, &parts));
}

// Period population explosion.
TEST(JwtTest, SplitJwtPeriodPopulationExplosionInvalid) {
  std::string jwt("......l....a...b.c.d.....");
  std::vector<std::string> parts;
  EXPECT_FALSE(Jwt::split_jwt(jwt, &parts));
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// b64_to_json tests
TEST(JwtTest, B64ToJsonSuccess) {
  std::string b64("eyJ2YWxpZCI6Impzb24ifQ");
  Json::Value value;
  std::unique_ptr<Json::CharReader> reader = Jwt::create_json_reader();
  EXPECT_TRUE(Jwt::b64_to_json(b64, reader.get(), &value));
  Json::Value expected;
  expected["valid"] = "json";
  EXPECT_EQ(expected, value);
}

TEST(JwtTest, B64ToJsonInvalidB64) {
  std::string b64("yummy");
  EXPECT_FALSE(Jwt::b64_to_json(b64, nullptr, nullptr));
}

TEST(JwtTest, B64ToJsonInvalidJson) {
  std::string b64("aW52YWxpZCBqc29u");
  Json::Value value;
  std::unique_ptr<Json::CharReader> reader = Jwt::create_json_reader();
  EXPECT_FALSE(Jwt::b64_to_json(b64, reader.get(), &value));
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// parse_header_and_payload tests
TEST(JwtTest, ParseHeaderAndPayloadSuccess) {
  std::string header_b64("eyJJIGFtIGEiOiJoZWFkZXIifQ");
  std::string payload_b64("eyJJJ20gYSI6InBheWxvYWQifQ");
  Json::Value header, payload;
  EXPECT_TRUE(
      Jwt::parse_header_and_payload(
          header_b64, payload_b64, &header, &payload));
  Json::Value expected_header;
  expected_header["I am a"] = "header";
  EXPECT_EQ(expected_header, header);
  Json::Value expected_payload;
  expected_payload["I'm a"] = "payload";
  EXPECT_EQ(expected_payload, payload);
}

TEST(JwtTest, ParseHeaderAndPayloadBadHeader) {
  std::string header_b64("yummy");
  std::string payload_b64("eyJJJ20gYSI6InBheWxvYWQifQ");
  Json::Value header, payload;
  EXPECT_FALSE(
      Jwt::parse_header_and_payload(
          header_b64, payload_b64, &header, &payload));
}

TEST(JwtTest, ParseHeaderAndPayloadBadPayload) {
  std::string header_b64("eyJJIGFtIGEiOiJoZWFkZXIifQ");
  std::string payload_b64("yummy");
  Json::Value header, payload;
  EXPECT_FALSE(
      Jwt::parse_header_and_payload(
          header_b64, payload_b64, &header, &payload));
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// extract_string tests
TEST(JwtTest, ExtractStringSuccess) {
  const std::string expected = "a string!";
  Json::Value json;
  json["key"] = expected;
  std::string out;
  EXPECT_TRUE(Jwt::extract_string("key", json, &out));
  EXPECT_EQ(expected, out);
}

TEST(JwtTest, ExtractStringNonexistent) {
  Json::Value json;
  std::string out;
  EXPECT_FALSE(Jwt::extract_string("key", json, &out));
}

TEST(JwtTest, ExtractStringTypeMismatch1) {
  Json::Value json;
  json["key"] = 42;  // An integer is not a string.
  std::string out;
  EXPECT_FALSE(Jwt::extract_string("key", json, &out));
}

TEST(JwtTest, ExtractStringTypeMismatch2) {
  Json::Value json, value;
  value["foo"] = "bar";
  json["key"] = value;  // A JSON object is not a string.
  std::string out;
  EXPECT_FALSE(Jwt::extract_string("key", json, &out));
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// extract_uint64 tests
TEST(JwtTest, ExtractUint64Success) {
  const uint64_t expected = 1;
  Json::Value json;
  json["key"] = expected;
  uint64_t out;
  EXPECT_TRUE(Jwt::extract_uint64("key", json, &out));
  EXPECT_EQ(expected, out);
}

TEST(JwtTest, ExtractUint64Nonexistent) {
  Json::Value json;
  uint64_t out;
  EXPECT_FALSE(Jwt::extract_uint64("key", json, &out));
}

TEST(JwtTest, ExtractUint64NegativeValue) {
  const int64_t expected = -1;
  Json::Value json;
  json["key"] = expected;
  uint64_t out;
  EXPECT_FALSE(Jwt::extract_uint64("key", json, &out));
}

TEST(JwtTest, ExtractUint64TypeMismatch) {
  Json::Value json;
  json["key"] = "uint64";
  uint64_t out;
  EXPECT_FALSE(Jwt::extract_uint64("key", json, &out));
}
////////////////////////////////////////////////////////////////////////////////

}  // namespace
}  // namespace iap
}  // namespace cloud
}  // namespace google
