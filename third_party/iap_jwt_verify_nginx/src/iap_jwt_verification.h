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

#ifndef NGINX_IAP_JWT_VERIFY_IAP_JWT_VERIFICATION_H
#define NGINX_IAP_JWT_VERIFY_IAP_JWT_VERIFICATION_H

#include "src/jwt.h"
#include "src/types.h"

namespace google {
namespace cloud {
namespace iap {

bool iap_jwt_is_valid(const char *raw_jwt,
                      size_t raw_jwt_len,
                      uint64_t now,
                      const char *expected_aud,
                      size_t expected_aud_len,
                      std::shared_ptr<iap_key_map_t> keys);

bool verify_iap_jwt_sig(const Jwt &jwt, const iap_key_map_t &keys);

std::unique_ptr<uint8_t []> jose_sig_to_der_sig(
    const uint8_t *jose_sig, size_t jose_sig_len, size_t *der_sig_len);

std::shared_ptr<iap_key_map_t> load_keys(const char *file_name,
                                         size_t file_name_len);

}  // namespace iap
}  // namespace cloud
}  // namespace google

#endif  // NGINX_IAP_JWT_VERIFY_IAP_JWT_VERIFICATION_H

