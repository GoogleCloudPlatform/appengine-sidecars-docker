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

#include "third_party/iap_jwt_verify_nginx/src/base64_util.h"

namespace google {
namespace cloud {
namespace iap {

std::unique_ptr<Jwt> Jwt::parse_from_string(const std::string &jwt_str) {
  std::vector<std::string> parts;
  if (!split_jwt(jwt_str, &parts)) {
    return nullptr;
  }

  Json::Value header;
  Json::Value payload;
  if (!parse_header_and_payload(parts[0], parts[1], &header, &payload)) {
    return nullptr;
  }

  std::string alg;
  if (!extract_string("alg", header, &alg)) {
    return nullptr;
  }

  std::string kid;
  if (!extract_string("kid", header, &kid)) {
    return nullptr;
  }

  uint64_t iat;
  if (!extract_uint64("iat", payload, &iat)) {
    return nullptr;
  }

  uint64_t exp;
  if (!extract_uint64("exp", payload, &exp)) {
    return nullptr;
  }

  std::string aud;
  if (!extract_string("aud", payload, &aud)) {
    return nullptr;
  }

  size_t signature_length;
  std::unique_ptr<const uint8_t[]>
      signature_bytes = url_safe_base64_decode(parts[2], &signature_length);
  if (signature_bytes == nullptr) {
    return false;
  }

  return std::unique_ptr<Jwt>(new (std::nothrow) Jwt(
      std::move(parts),
      std::move(header),
      std::move(payload),
      std::move(signature_bytes),
      signature_length,
      std::move(alg),
      std::move(kid),
      iat,
      exp,
      std::move(aud)));
}

bool Jwt::split_jwt(const std::string &jwt_str,
                    std::vector<std::string> *jwt_parts) {
  size_t len = jwt_str.length();
  unsigned int period_count = 0;
  size_t first_period_pos, second_period_pos;
  for (size_t i = 0; i < len; i++) {
    if (jwt_str[i] == '.') {
      if (++period_count > 2) return false;
      switch (period_count) {
        case 1:
          // No initial periods
          if (i == 0) return false;
          first_period_pos = i;
          break;
        case 2:
          // No ending periods
          if (i == len - 1) return false;
          // No adjacent periods
          if (i - first_period_pos == 1) return false;
          second_period_pos = i;
          break;
      }
    }
  }

  // Catches zero or one period
  if (period_count != 2) return false;

  jwt_parts->resize(3);
  (*jwt_parts)[0] = jwt_str.substr(0, first_period_pos);
  (*jwt_parts)[1] = jwt_str.substr(
      first_period_pos + 1, second_period_pos - first_period_pos - 1);
  (*jwt_parts)[2] = jwt_str.substr(
      second_period_pos + 1, len - second_period_pos - 1);
  return true;
}

std::unique_ptr<Json::CharReader> Jwt::create_json_reader() {
  Json::CharReaderBuilder json_reader_builder;
  json_reader_builder["allowSingleQuotes"] = true;
  std::unique_ptr<Json::CharReader> json_reader(
      json_reader_builder.newCharReader());
  return json_reader;
}

bool Jwt::b64_to_json(
    const std::string &b64_str,
    Json::CharReader *json_reader,
    Json::Value *value) {
  std::string decoded;
  if (!url_safe_base64_decode_to_string(b64_str, &decoded)) {
    return false;
  }

  std::string errs;
  return json_reader->parse(
      decoded.data(), decoded.data() + decoded.length(), value, &errs);
}

bool Jwt::parse_header_and_payload(
    const std::string& header_b64,
    const std::string& payload_b64,
    Json::Value *header,
    Json::Value *payload) {
  std::unique_ptr<Json::CharReader> json_reader = create_json_reader();
  return (json_reader != nullptr
          && b64_to_json(header_b64, json_reader.get(), header)
          && b64_to_json(payload_b64, json_reader.get(), payload));
}

bool Jwt::extract_string(
          const std::string key, const Json::Value& payload, std::string *out) {
  const Json::Value &val = payload[key];
  if (!val.isString()) {
    return false;
  }

  *out = val.asString();
  return true;
}

bool Jwt::extract_uint64(
          const std::string key, const Json::Value& payload, uint64_t *out) {
  const Json::Value &val = payload[key];
  if (!val.isUInt64()) {
    return false;
  }

  *out = val.asUInt64();
  return true;
}

Jwt::Jwt(const std::vector<std::string> &&parts,
         const Json::Value &&header,
         const Json::Value &&payload,
         std::unique_ptr<const uint8_t[]> signature_bytes,
         const size_t signature_length,
         const std::string &&alg,
         const std::string &&kid,
         const uint64_t iat,
         const uint64_t exp,
         const std::string &&aud) :
    parts_(parts),
    header_(header),
    payload_(payload),
    signature_bytes_(std::move(signature_bytes)),
    signature_length_(signature_length),
    alg_(alg),
    kid_(kid),
    iat_(iat),
    exp_(exp),
    aud_(aud) {
}

}  // namespace iap
}  // namespace cloud
}  // namespace google
