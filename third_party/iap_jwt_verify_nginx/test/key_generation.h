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

#ifndef NGINX_IAP_JWT_VERIFY_TEST_KEY_GENERATION_H
#define NGINX_IAP_JWT_VERIFY_TEST_KEY_GENERATION_H

#include "json/json.h"
#include "openssl/ec_key.h"

namespace google {
namespace cloud {
namespace iap {
namespace test {

std::string bignum_to_b64(const BIGNUM *bn);

// This one allocates new BIGNUMs
void get_pubkey_points(EC_KEY *key, BIGNUM **x, BIGNUM **y);

// This one assumes x and y point to valid BIGNUMs
void get_pubkey_points(EC_KEY *key, BIGNUM *x, BIGNUM *y);

bssl::UniquePtr<EC_KEY> gen_P256_key();

Json::Value ec_key_to_jwk(EC_KEY *key, std::string kid);

void create_key_files(std::string pub_key_fname, std::string priv_key_fname);

Json::Value gen_jwk(std::string kid);
Json::Value gen_jwk();

}  // namespace test
}  // namespace iap
}  // namespace cloud
}  // namespace google

#endif  // NGINX_IAP_JWT_VERIFY_TEST_KEY_GENERATION_H
