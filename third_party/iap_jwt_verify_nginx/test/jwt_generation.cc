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

#include "test/jwt_generation.h"

#include <fstream>
#include <memory>
#include <sstream>

#include "openssl/base64.h"
#include "openssl/bn.h"
#include "openssl/bytestring.h"
#include "openssl/ec_key.h"
#include "openssl/ecdsa.h"
#include "openssl/sha.h"
#include "src/key_util.h"
#include "test/b64_encode.h"

namespace google {
namespace cloud {
namespace iap {
namespace test {

// Length of the elements of the finite field used for signing.
constexpr unsigned int FINITE_FIELD_BYTE_SIZE = 32;

using std::string;

string generate_iap_jwt(Json::Value &header, Json::Value &payload) {
  Json::StreamWriterBuilder builder;
  builder["commentStyle"] = "None";
  builder["indentation"] = "";
  std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());

  std::stringstream jwt(""), ss("");
  writer->write(header, &ss);
  jwt << url_safe_b64_encode(ss.str());
  jwt << ".";

  ss.str("");
  writer->write(payload, &ss);
  jwt << url_safe_b64_encode(ss.str());

  string sig = sign_iap_jwt(jwt.str(), header["kid"].asString());

  jwt << ".";
  jwt << sig;
  return jwt.str();
}

string sign_iap_jwt(string header_dot_payload, string kid) {
  Json::Value jwks;
  Json::Value private_keys;
  std::fstream("test/keys.jwk") >> jwks;
  std::fstream("test/keys.priv") >> private_keys;
  std::string encoded_priv_key = private_keys[kid].asString();
  Json::Value jwk_list = jwks["keys"];
  Json::Value jwk;
  for (size_t i = 0; i < jwk_list.size(); i++ ) {
    if (jwk_list.get(i, Json::Value())["kid"] == kid) {
      jwk = jwk_list.get(i, Json::Value());
      break;
    }
  }

  std::shared_ptr<EC_KEY> key = ec_key_from_jwk(jwk);
  bssl::UniquePtr<BIGNUM> priv_key = b64_to_bignum(encoded_priv_key);
  EC_KEY_set_private_key(key.get(), priv_key.get());

  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const uint8_t *>(header_dot_payload.data()),
         header_dot_payload.length(),
         digest);
  std::unique_ptr<uint8_t[]> der_sig(new uint8_t[ECDSA_size(key.get())]);
  unsigned int der_sig_len;
  ECDSA_sign(
      0, digest, SHA256_DIGEST_LENGTH, der_sig.get(), &der_sig_len, key.get());
  unsigned int sig_len;
  std::unique_ptr<uint8_t[]> sig =
      der_sig_to_jose_sig(der_sig.get(), der_sig_len, &sig_len);
  return url_safe_b64_encode(sig.get(), sig_len);
}

namespace {
  void copy_der_int_cbs_to_array(CBS *cbs, uint8_t *out) {
    const size_t len = CBS_len(cbs);
    if (len == FINITE_FIELD_BYTE_SIZE) {
      CBS_copy_bytes(cbs, out, len);
    } else if (len == FINITE_FIELD_BYTE_SIZE + 1) {
      // Since DER integers are signed, sometimes there's an extra leading zero
      // we need to skip.
      CBS_skip(cbs, 1);
      CBS_copy_bytes(cbs, out, len - 1);
    } else {
      // Boom.
      abort();
    }
  }
}

std::unique_ptr<uint8_t[]> der_sig_to_jose_sig(
    const uint8_t *der_sig, unsigned int der_sig_len, unsigned int *sig_len) {
  CBS der_sig_cbs;
  CBS_init(&der_sig_cbs, der_sig, der_sig_len);
  CBS seq, r, s;
  CBS_get_asn1(&der_sig_cbs, &seq, CBS_ASN1_SEQUENCE);
  CBS_get_asn1(&seq, &r, CBS_ASN1_INTEGER);
  CBS_get_asn1(&seq, &s, CBS_ASN1_INTEGER);
  *sig_len = 2*FINITE_FIELD_BYTE_SIZE;
  std::unique_ptr<uint8_t[]> sig(new uint8_t[*sig_len]);
  copy_der_int_cbs_to_array(&r, sig.get());
  copy_der_int_cbs_to_array(&s, sig.get() + FINITE_FIELD_BYTE_SIZE);
  return sig;
}

}  // namespace test
}  // namespace iap
}  // namespace cloud
}  // namespace google
