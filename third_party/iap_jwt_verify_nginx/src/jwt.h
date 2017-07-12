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

#ifndef NGINX_IAP_JWT_VERIFY_JWT_H
#define NGINX_IAP_JWT_VERIFY_JWT_H

#include <memory>
#include <string>
#include <vector>

#include "json/json.h"

namespace google {
namespace cloud {
namespace iap {

class Jwt {
 public:
  Jwt() = delete;
  Jwt(const Jwt &) = delete;
  Jwt(Jwt &&) = default;
  Jwt &operator=(const Jwt &) = delete;
  Jwt &operator=(Jwt &&) = default;

  // Creates a Jwt based on the provided string. Returns a nullptr on failure.
  static std::unique_ptr<Jwt> parse_from_string(const std::string &jwt_str);

  // Splits JWT into three pieces.
  // Returns true on success, false on failure.
  // Uses the provided vector to store the JWT parts as follows:
  // jwt_parts[0] --> header
  // jwt_parts[1] --> payload
  // jwt_parts[2] --> signature
  // If true is returned, jwt_parts is guaranteed to contain three non-empty
  // string elements.
  static bool split_jwt(
      const std::string &raw_jwt, std::vector<std::string> *jwt_parts);

  // Create a JSON parser with appropriate arguments.
  static std::unique_ptr<Json::CharReader> create_json_reader();

  // Converts a base 64-encoded string to a JSON value using the provided
  // reader. Returns true on success, false on failure.
  static bool b64_to_json(
      const std::string &b64_string,
      Json::CharReader *json_reader,
      Json::Value *value);

  // Converts the b64-encoded header and payload to JSON representations.
  // Returns true for success, false for failure.
  static bool parse_header_and_payload(
      const std::string& header_b64,
      const std::string& payload_b64,
      Json::Value *header,
      Json::Value *payload);

  // Attempts to extract a string value from the supplied Json object with the
  // given key. Returns true on success, false on failure.
  static bool extract_string(
      const std::string key, const Json::Value &payload, std::string *out);

  // Attempts to extract a uint64 value from the supplied Json object with the
  // given key. Returns true on success, false on failure.
  static bool extract_uint64(
      const std::string key, const Json::Value &payload, uint64_t *out);

  const std::string &encoded_header() const { return parts_[0]; }
  const std::string &encoded_payload() const { return parts_[1]; }
  const Json::Value &header() const { return header_; }
  const Json::Value &payload() const { return payload_; }
  const uint8_t *signature_bytes() const { return signature_bytes_.get(); }
  const size_t signature_length() const { return signature_length_; }
  const std::string &alg() const { return alg_; }
  const std::string &kid() const { return kid_; }
  uint64_t iat() const { return iat_; }
  uint64_t exp() const { return exp_; }
  const std::string &aud() const { return aud_; }

 private:
  Jwt(const std::vector<std::string> &&parts,
      const Json::Value &&header,
      const Json::Value &&payload,
      std::unique_ptr<const uint8_t[]> signature_bytes,
      size_t signature_length,
      const std::string &&alg,
      const std::string &&kid,
      uint64_t iat,
      uint64_t exp,
      const std::string &&aud);

  std::vector<std::string> parts_;
  Json::Value header_;
  Json::Value payload_;
  const std::unique_ptr<const uint8_t[]> signature_bytes_;
  const size_t signature_length_;
  const std::string alg_;
  const std::string kid_;
  const uint64_t iat_;
  const uint64_t exp_;
  const std::string aud_;
};

}  // namespace iap
}  // namespace cloud
}  // namespace google

#endif  // NGINX_IAP_JWT_VERIFY_JWT_H

