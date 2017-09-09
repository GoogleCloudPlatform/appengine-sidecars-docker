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

#include "third_party/iap_jwt_verify_nginx/src/base64_util.h"

#include <string>

#include "gtest/gtest.h"

namespace google {
namespace cloud {
namespace iap {
namespace {

////////////////////////////////////////////////////////////////////////////////
// max_decoded_length tests
TEST(Base64UtilTest, MaxDecodedLengthMod4Eq0Valid) {
  std::string b64("abcd");
  size_t max_decoded_len;
  EXPECT_TRUE(max_decoded_length(b64, &max_decoded_len));
  EXPECT_EQ(3, max_decoded_len);
}

TEST(Base64UtilTest, MaxDecodedLengthMod4Eq1Invalid) {
  std::string b64("a");
  size_t max_decoded_len;
  EXPECT_FALSE(max_decoded_length(b64, &max_decoded_len));
}

TEST(Base64UtilTest, MaxDecodedLengthMod4Eq2Valid) {
  std::string b64("ab");
  size_t max_decoded_len;
  EXPECT_TRUE(max_decoded_length(b64, &max_decoded_len));
  EXPECT_EQ(3, max_decoded_len);
}

TEST(Base64UtilTest, MaxDecodedLengthMod4Eq3Valid) {
  std::string b64("abc");
  size_t max_decoded_len;
  EXPECT_TRUE(max_decoded_length(b64, &max_decoded_len));
  EXPECT_EQ(3, max_decoded_len);
}

TEST(Base64UtilTest, MaxDecodedLengthLongString0) {
  std::string b64("19p8138ASDF9gedsaf9081h23f-_9ash_fas98fh");
  size_t max_decoded_len;
  EXPECT_TRUE(max_decoded_length(b64, &max_decoded_len));
  EXPECT_EQ(30, max_decoded_len);
}

TEST(Base64UtilTest, MaxDecodedLengthLongString1) {
  std::string b64("19p8138ASDF9gedsaf9081h23f-_9ash_fas98fhB");
  size_t max_decoded_len;
  EXPECT_FALSE(max_decoded_length(b64, &max_decoded_len));
}

TEST(Base64UtilTest, MaxDecodedLengthLongString2) {
  std::string b64("19p8138ASDF9gedsaf9081h23f-_9ash_fas98fhBB");
  size_t max_decoded_len;
  EXPECT_TRUE(max_decoded_length(b64, &max_decoded_len));
  EXPECT_EQ(33, max_decoded_len);
}

TEST(Base64UtilTest, MaxDecodedLengthLongString3) {
  std::string b64("19p8138ASDF9gedsaf9081h23f-_9ash_fas98fhBBB");
  size_t max_decoded_len;
  EXPECT_TRUE(max_decoded_length(b64, &max_decoded_len));
  EXPECT_EQ(33, max_decoded_len);
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// canonicalize_b64_string tests
TEST(Base64UtilTest, CanonicalizeB64StringUnchanged) {
  const std::string b64(
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789==");
  EXPECT_EQ(canonicalize_b64_string(b64), b64);
}

TEST(Base64UtilTest, CanonicalizeB64StringAddOnePaddingChar) {
  const std::string b64("abc");
  EXPECT_EQ(canonicalize_b64_string(b64), "abc=");
}

TEST(Base64UtilTest, CanonicalizeB64StringAddTwoPaddingChars) {
  const std::string b64("ab");
  EXPECT_EQ(canonicalize_b64_string(b64), "ab==");
}

TEST(Base64UtilTest, CanonicalizeB64StringConvertMinusToPlus) {
  const std::string b64("-xyz");
  EXPECT_EQ(canonicalize_b64_string(b64), "+xyz");
}

TEST(Base64UtilTest, CanonicalizeB64StringConvertUnderscoreToSlash) {
  const std::string b64("_xyz");
  EXPECT_EQ(canonicalize_b64_string(b64), "/xyz");
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// url_safe_base64_decode (first overload) tests
// Coverage is somewhat light since the implementation relies on the BoringSSL
// implementation and canonicalized_b64_string, so it contains no real logic.
TEST(Base64UtilTest, UrlSafeBase64DecodeCase1) {
  const std::string test_b64("dGhpcyBpcyBhIHRlc3Qgc3RyaW5n");
  size_t max_decoded_len;
  EXPECT_TRUE(max_decoded_length(test_b64, &max_decoded_len));
  uint8_t *decoded = new uint8_t[max_decoded_len];
  size_t decoded_len;
  EXPECT_TRUE(url_safe_base64_decode(
      test_b64,
      max_decoded_len,
      decoded,
      &decoded_len));
  EXPECT_EQ(decoded_len, max_decoded_len);
  std::string decoded_as_string(
      reinterpret_cast<const char *>(decoded), decoded_len);
  EXPECT_EQ(decoded_as_string, "this is a test string");
  delete [] decoded;
}

TEST(Base64UtilTest, UrlSafeBase64DecodeCase2) {
  const std::string test_b64(
      "dGhpcyBpcyBhIHRlc3Qgc3RyaW5ndGhpcyBpcyBhIHRlc3Qgc3RyaW5n");
  size_t max_decoded_len;
  EXPECT_TRUE(max_decoded_length(test_b64, &max_decoded_len));
  uint8_t *decoded = new uint8_t[max_decoded_len];
  size_t decoded_len;
  EXPECT_TRUE(url_safe_base64_decode(
      test_b64,
      max_decoded_len,
      decoded,
      &decoded_len));
  EXPECT_EQ(decoded_len, max_decoded_len);
  std::string decoded_as_string(
      reinterpret_cast<const char *>(decoded), decoded_len);
  EXPECT_EQ(decoded_as_string, "this is a test stringthis is a test string");
  delete [] decoded;
}

TEST(Base64UtilTest, UrlSafeBase64DecodeCase3) {
  const std::string test_b64("YWI=");
  size_t max_decoded_len;
  EXPECT_TRUE(max_decoded_length(test_b64, &max_decoded_len));
  uint8_t *decoded = new uint8_t[max_decoded_len];
  size_t decoded_len;
  EXPECT_TRUE(url_safe_base64_decode(
      test_b64,
      max_decoded_len,
      decoded,
      &decoded_len));
  EXPECT_EQ(decoded_len, max_decoded_len - 1);
  std::string decoded_as_string(
      reinterpret_cast<const char *>(decoded), decoded_len);
  EXPECT_EQ(decoded_as_string, "ab");
  delete [] decoded;
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// url_safe_base64_decoded (second overload) tests
TEST(Base64UtilTest, UrlSafeBase64DecodeSuccess) {
  // This decodes to a byte array of length 256 containing the integers zero
  // through two hundred fifty-five in ascending order.
  const std::string b64_str("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0-P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn-AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq-wsbKztLW2t7i5uru8vb6_wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t_g4eLj5OXm5-jp6uvs7e7v8PHy8_T19vf4-fr7_P3-_w");
  size_t decoded_length;
  std::unique_ptr<uint8_t[]> decoded =
      url_safe_base64_decode(b64_str, &decoded_length);
  EXPECT_TRUE(decoded != nullptr);
  EXPECT_EQ(256, decoded_length);
  for (size_t i = 0; i < decoded_length; i++) {
    EXPECT_EQ(i, decoded[i]);
  }
}

TEST(Base64UtilTest, UrlSafeBase64DecodeBadB64) {
  const std::string b64_str("yummy");
  size_t decoded_length;
  std::unique_ptr<uint8_t[]> decoded =
      url_safe_base64_decode(b64_str, &decoded_length);
  EXPECT_EQ(nullptr, decoded);
}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// url_safe_base64_decode_to_string tests
TEST(Base64UtilTest, UrlSafeBase64DecodeToStringCase1) {
  const std::string b64("dGhpcyBpcyBhIHRlc3Qgc3RyaW5n");
  std::string decoded;
  EXPECT_TRUE(url_safe_base64_decode_to_string(b64, &decoded));
  EXPECT_EQ(decoded, "this is a test string");
}

TEST(Base64UtilTest, UrlSafeBase64DecodeToStringInvalidBase64Length) {
  const std::string b64("dGhpc");
  std::string decoded;
  EXPECT_FALSE(url_safe_base64_decode_to_string(b64, &decoded));
}

TEST(Base64UtilTest, UrlSafeBase64DecodeToStringInvalidBase64Character) {
  const std::string b64("dGhpc67%");
  std::string decoded;
  EXPECT_FALSE(url_safe_base64_decode_to_string(b64, &decoded));
}
////////////////////////////////////////////////////////////////////////////////

}  // namespace
}  // namespace iap
}  // namespace cloud
}  // namespace google
