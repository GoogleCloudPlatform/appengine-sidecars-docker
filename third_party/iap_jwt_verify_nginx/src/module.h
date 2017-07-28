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

#ifndef NGINX_IAP_JWT_VERIFY_MODULE_H
#define NGINX_IAP_JWT_VERIFY_MODULE_H

#include <atomic>

#include "src/types.h"

extern "C" {
#include "src/http/ngx_http.h"
}

namespace google {
namespace cloud {
namespace iap {

// IAP JWT verification module location context
// This represents configuration at the location block level.
typedef struct {
  // Whether or not to check requests for valid IAP JWTs.
  ngx_flag_t iap_jwt_verify;
} ngx_iap_jwt_verify_loc_conf_t;

// IAP JWT verification module main context
// This is for application-scoped values (i.e. independent of location)
typedef struct {
  // The project number of the associated GCP project. A component of the JWT
  // audience.
  ngx_str_t project_number;

  // The app id of the associated project (identical to the project id).
  ngx_str_t app_id;

  // The path to the file containing the verification keys.
  ngx_str_t key_file;

  // The path to the file containing the current IAP state.
  ngx_str_t iap_state_file;

  // How long to wait between checking for the presence/absence of the state
  // file. Maximum value: five minutes. Default value: five minutes. Providing a
  // value < 0 or > 300 seconds results in a config error. Providing a value
  // of zero will cause the state to be checked continuously (but threads will
  // not block if they fail to acquire the lock).
  ngx_int_t iap_state_cache_time_sec;

  // How long to cache the verification keys before attempting to refresh.
  // Maximum value: half a day. Default value: half a day.
  ngx_int_t key_cache_time_sec;

  // Indicates whether IAP JWT verfication is enabled at all--if false at the
  // postconfiguration step, we don't even bother to insert the handler.
  bool module_in_use;

  // Whether or not IAP is on (if IAP is off, no checks will be performed).
  // This value is not stored in configuration; rather, it is deduced at
  // runtime from the presence/absence of of the iap_state_file.
  volatile bool iap_on;

  // Used to mark whether we are in a fail-open regime on account of the state
  // file not having been modified recently enough.
  volatile bool fail_open_because_state_stale;

  // Time (in seconds since epoch start) that the IAP state was last checked.
  std::atomic<time_t> last_iap_state_check;

  // The expected audience for the JWTs. Calculated from project_number and
  // app_id, rather than configured.
  ngx_str_t expected_aud;

  // Shared pointer to all currently valid certificates.
  // Access to this object is synchronized; see implementation.
  std::shared_ptr<iap_key_map_t> key_map;

  // Time at which the key map was last updated.
  volatile time_t last_key_map_update;
} ngx_iap_jwt_verify_main_conf_t;

}  // namespace iap
}  // namespace cloud
}  // namespace google

extern ngx_module_t ngx_iap_jwt_verify_module;

#endif  // NGINX_IAP_JWT_VERIFY_MODULE_H

