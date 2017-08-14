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

#include "third_party/iap_jwt_verify_nginx/test/b64_encode.h"

#include "openssl/base64.h"

namespace google {
namespace cloud {
namespace iap {
namespace test {

using std::string;

string url_safe_b64_encode(const string &to_encode) {
  return url_safe_b64_encode(
      reinterpret_cast<const uint8_t *>(to_encode.data()), to_encode.length());
}

string url_safe_b64_encode(const uint8_t *to_encode, const size_t len) {
  size_t encoded_len;
  EVP_EncodedLength(&encoded_len, len);
  uint8_t *output = new uint8_t[encoded_len];
  EVP_EncodeBlock(output, to_encode, len);

  // Note: BoringSSL null-terminates the output; subtract one from the encoded
  // length in order to ignore this.
  encoded_len--;
  string result(reinterpret_cast<const char *>(output), encoded_len);
  delete [] output;
  for (size_t i = 0; i < encoded_len; i++) {
    if (result[i] == '+') result[i] = '-';
    if (result[i] == '/') result[i] = '_';
  }

  if (result[encoded_len - 2] == '=') {
    return result.substr(0, encoded_len - 2);
  }

  if (result[encoded_len - 1] == '=') {
    return result.substr(0, encoded_len - 1);
  }

  return result;
}

}  // namespace test
}  // namespace iap
}  // namespace cloud
}  // namespace google
