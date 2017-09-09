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

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "openssl/bn.h"
#include "openssl/bytestring.h"
#include "openssl/ecdsa.h"
#include "openssl/sha.h"
#include "third_party/iap_jwt_verify_nginx/src/jwt.h"
#include "third_party/iap_jwt_verify_nginx/src/key_util.h"

namespace google {
namespace cloud {
namespace iap {

// Let's refuse to even process JWTs larger than a certain size.
constexpr size_t MAX_RAW_JWT_SIZE = 4096;

// Allow for one minute of clock skew when verifying IAP JWT iat/exp values.
constexpr uint64_t CLOCK_SKEW = 60;

// Length of the elements of the finite field used for signing.
constexpr unsigned int FINITE_FIELD_BYTE_SIZE = 32;

bool iap_jwt_is_valid(const char *const raw_jwt,
                      size_t raw_jwt_len,
                      uint64_t now,
                      char const *const expected_aud,
                      size_t expected_aud_len,
                      std::shared_ptr<iap_key_map_t> keys) {
  if (raw_jwt == nullptr || raw_jwt_len > MAX_RAW_JWT_SIZE) {
    return false;
  }

  std::unique_ptr<Jwt> jwt =
      Jwt::parse_from_string(std::string(raw_jwt, raw_jwt_len));
  if (jwt == nullptr) {
    return false;
  }

  if (now < jwt->iat() - CLOCK_SKEW || now > jwt->exp() + CLOCK_SKEW) {
    return false;
  }

  if (std::string(expected_aud, expected_aud_len) != jwt->aud()) {
    return false;
  }

  if (keys == nullptr) {
    return false;
  }

  if (!verify_iap_jwt_sig(*jwt, *keys)) {
    return false;
  }

  return true;
}

bool verify_iap_jwt_sig(const Jwt &jwt, const iap_key_map_t &keys) {
  iap_key_map_t::const_iterator key_iter = keys.find(jwt.kid());
  if (key_iter == keys.end()) {
    return false;
  }

  const std::string header_dot_payload =
      jwt.encoded_header() + "." + jwt.encoded_payload();
  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const uint8_t *>(header_dot_payload.data()),
         header_dot_payload.length(),
         digest);

  if (jwt.signature_length() != 2*FINITE_FIELD_BYTE_SIZE) {
    return false;
  }

  const uint8_t *const sig_bytes = jwt.signature_bytes();
  bssl::UniquePtr<ECDSA_SIG> ecdsa_sig(ECDSA_SIG_new());
  BN_bin2bn(sig_bytes, FINITE_FIELD_BYTE_SIZE, ecdsa_sig->r);
  BN_bin2bn(
      sig_bytes + FINITE_FIELD_BYTE_SIZE, FINITE_FIELD_BYTE_SIZE, ecdsa_sig->s);
  return ECDSA_do_verify(digest,
                         SHA256_DIGEST_LENGTH,
                         ecdsa_sig.get(),
                         key_iter->second.get()) == 1;
}

std::shared_ptr<iap_key_map_t> load_keys(const char *file_name,
                                         const size_t file_name_len) {
  std::ifstream file(std::string(file_name, file_name_len));
  if (!file.is_open()) {
    return nullptr;
  }

  Json::Value jwks;
  try {
    file >> jwks;
  } catch (std::exception) {
    return nullptr;
  }

  return process_keys(jwks["keys"]);
}

}  // namespace iap
}  // namespace cloud
}  // namespace google
