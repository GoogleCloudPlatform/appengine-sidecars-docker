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

#include "src/iap_jwt_verification.h"

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "openssl/bn.h"
#include "openssl/bytestring.h"
#include "openssl/ecdsa.h"
#include "openssl/sha.h"
#include "src/jwt.h"
#include "src/key_util.h"

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

  if (now < jwt->iat() - 60 || now > jwt->exp() + 60) {
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
  size_t der_sig_len;
  std::unique_ptr<const uint8_t []> der_sig = jose_sig_to_der_sig(
      jwt.signature_bytes(), jwt.signature_length(), &der_sig_len);
  if (der_sig == nullptr) {
    return false;
  }

  if (ECDSA_verify(0,
                   digest,
                   SHA256_DIGEST_LENGTH,
                   der_sig.get(),
                   der_sig_len,
                   key_iter->second.get()) != 1) {
    return false;
  }

  return true;
}

// Most of this implementation is copied from the BoringSSL code that creates
// DER-encoded signatures in the first place--see crypto/dsa/dsa_asn1.c in the
// BoringSSL codebase.
std::unique_ptr<uint8_t []> jose_sig_to_der_sig(
    const uint8_t *const jose_sig,
    const size_t jose_sig_len,
    size_t *const der_sig_len) {
  if (jose_sig_len != 2*FINITE_FIELD_BYTE_SIZE) {
    return nullptr;
  }

  // Convert the two components of the jose signature into BIGNUMs
  bssl::UniquePtr<BIGNUM> r(
      BN_bin2bn(jose_sig, FINITE_FIELD_BYTE_SIZE, nullptr));
  if (r == nullptr) {
    return nullptr;
  }

  bssl::UniquePtr<BIGNUM> s(
      BN_bin2bn(
          jose_sig + FINITE_FIELD_BYTE_SIZE, FINITE_FIELD_BYTE_SIZE, nullptr));
  if (s == nullptr) {
    return false;
  }

  CBB der_sig_cbb;

  // Second argument is only a guideline--we know the resulting ASN.1 will have
  // to contain the two 32-byte integers, as well as some overhead for the
  // encoding. The suggested space should more than cover this.
  if (CBB_init(&der_sig_cbb, 2*FINITE_FIELD_BYTE_SIZE + 20) != 1) {
    CBB_cleanup(&der_sig_cbb);
    return nullptr;
  }

  CBB child;
  if (CBB_add_asn1(&der_sig_cbb, &child, CBS_ASN1_SEQUENCE) != 1
      || BN_marshal_asn1(&child, r.get()) != 1
      || BN_marshal_asn1(&child, s.get()) != 1
      || CBB_flush(&der_sig_cbb) != 1) {
    CBB_cleanup(&der_sig_cbb);
    return nullptr;
  }

  *der_sig_len = CBB_len(&der_sig_cbb);
  std::unique_ptr<uint8_t []> der_sig(new (std::nothrow) uint8_t[*der_sig_len]);
  const uint8_t *der_sig_bytes = CBB_data(&der_sig_cbb);
  for (size_t i = 0; i < *der_sig_len; i++) {
    der_sig.get()[i] = der_sig_bytes[i];
  }

  CBB_cleanup(&der_sig_cbb);
  return der_sig;
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
