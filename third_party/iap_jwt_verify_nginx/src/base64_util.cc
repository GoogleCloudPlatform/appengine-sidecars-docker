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

#include "src/base64_util.h"

#include <string>

#include "openssl/base64.h"

namespace google {
namespace cloud {
namespace iap {

bool max_decoded_length(const std::string &b64_str, size_t *len) {
  size_t b64_str_len = b64_str.length();
  *len = 3 * (b64_str_len / 4);
  switch (b64_str_len % 4) {
    case 0:
      break;
    case 1:
      return false;
    default:
      *len += 3;
  }

  return true;
}

std::string canonicalize_b64_string(const std::string &b64_str) {
  size_t b64_str_len = b64_str.length();
  size_t canonicalized_b64_str_len = b64_str_len;
  switch (b64_str_len % 4) {
    case 2:
      canonicalized_b64_str_len += 2;
      break;
    case 3:
      canonicalized_b64_str_len += 1;
      break;
  }

  std::string canonicalized_b64_str(canonicalized_b64_str_len, '=');
  for (size_t i = 0; i < b64_str_len; i++) {
    char c = b64_str[i];
    if (c == '-') {
      canonicalized_b64_str[i] = '+';
    } else if (c == '_') {
      canonicalized_b64_str[i] = '/';
    } else {
      canonicalized_b64_str[i] = c;
    }
  }

  return canonicalized_b64_str;
}

// Reusing the boringssl base 64 function this way is not the most CPU-efficient
// thing to do since that implementation is constant-time for security reasons
// that don't apply here. It was a conscious trade-off to save development time.
// Eventually a better solution for base 64 should probably be found.
bool url_safe_base64_decode(
    const std::string &b64_str,
    size_t max_len,
    uint8_t *decoded,
    size_t *decoded_len) {
  std::string canonicalized_b64_str = canonicalize_b64_string(b64_str);
  int ret = EVP_DecodeBase64(
      decoded,
      decoded_len,
      max_len,
      (const uint8_t *)canonicalized_b64_str.data(),
      canonicalized_b64_str.length());
  return ret == 1;
}

std::unique_ptr<uint8_t[]> url_safe_base64_decode(
    const std::string &b64_str, size_t *decoded_len) {
  size_t max_len;
  if (!max_decoded_length(b64_str, &max_len)) {
    return nullptr;
  }

  std::unique_ptr<uint8_t[]> bytes(new (std::nothrow) uint8_t[max_len]);
  if (bytes == nullptr) {
    return nullptr;
  }

  if (!url_safe_base64_decode(b64_str, max_len, bytes.get(), decoded_len)) {
    return nullptr;
  }

  return bytes;
}

bool url_safe_base64_decode_to_string(
    const std::string &b64_str,
    std::string *out_str) {
  size_t decoded_len;
  std::unique_ptr<uint8_t[]> decoded = url_safe_base64_decode(
      b64_str, &decoded_len);
  if (decoded == nullptr) {
    return false;
  }

  *out_str = std::string(
      reinterpret_cast<const char *>(decoded.get()), decoded_len);
  return true;
}

}  // namespace iap
}  // namespace cloud
}  // namespace google
