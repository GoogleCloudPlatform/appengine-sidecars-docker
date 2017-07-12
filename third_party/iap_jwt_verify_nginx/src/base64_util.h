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

#ifndef NGINX_IAP_JWT_VERIFY_BASE64_UTIL_H
#define NGINX_IAP_JWT_VERIFY_BASE64_UTIL_H

#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace iap {

// Returns the maximum length the provided string might decode to. Strings with
// ommitted padding will work fine.
// Returns true if successful, false indicates that the string has a length
// incompatible with base 64 decoding.
bool max_decoded_length(const std::string &b64_str, size_t *len);

// Adds padding if necessary, and converts '-' --> '+', '_' --> '/'
std::string canonicalize_b64_string(const std::string &b64_str);

// Base 64 decode b64_str, using the character set:
// ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_
// (this is the "URL-safe" set, the normal set ends with "+/").
// It is okay if padding is omitted from the input string.
//
// - 'max_len' should be the length of the output buffer
// - 'decoded' will hold the decoded bytes
// - 'decoded_len' will indicate the number of bytes decoded
//
// Returns true if successful, false otherwise.
bool url_safe_base64_decode(
    const std::string &b64_str,
    size_t max_len,
    uint8_t *decoded,
    size_t *decoded_len);

// Similar to above, but removes the need for the caller to allocate memory.
// Returns nullptr for failure, or an std::unique_ptr to the decoded bytes if
// successful.
std::unique_ptr<uint8_t[]> url_safe_base64_decode(
    const std::string &b64_str, size_t *decoded_len);

// Convenience method for when it is desireable to store the output of base64
// decoding in an std::string. Returns true if successful, false otherwise.
bool url_safe_base64_decode_to_string(
    const std::string &b64_str, std::string *out_str);

}  // namespace iap
}  // namespace cloud
}  // namespace google

#endif  // NGINX_IAP_JWT_VERIFY_BASE64_UTIL_H
