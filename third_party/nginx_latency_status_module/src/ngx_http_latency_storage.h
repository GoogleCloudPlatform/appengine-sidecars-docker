// Copyright (C) 2002-2016 Igor Sysoev
// Copyright (C) 2011-2016 Nginx, Inc.
// Copyright (C) 2020 Google Inc.
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

#ifndef _NGX_HTTP_LATENCY_STORAGE_H_INCLUDED_
#define _NGX_HTTP_LATENCY_STORAGE_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


// The config struct for the location config.
typedef struct {
  ngx_flag_t receiver_enabled;
  ngx_flag_t status_page_enabled;
} ngx_http_latency_conf_t;

// A struct to store the main confg.
typedef struct {
  ngx_flag_t enabled;
  ngx_shm_zone_t *shm_zone;  // a pointer to the shared memory containing the latency stats

  // These values define the bucket boundaries used by the latency distribution.
  // There will be max_exponent + 2 buckets with the boundaries
  // [0, scaled_factor * base ^ i) for i = 0
  // [scaled_factor * base ^ (i - 1), scaled_factor * base ^ i ) for 0 < i <= max_exponent
  // [scaled_factor * base ^ (i - 1), infinity) for i = max_exponent + 1
  ngx_int_t base;
  ngx_int_t scale_factor;
  ngx_int_t max_exponent;
  ngx_int_t *latency_bucket_bounds;
} ngx_http_latency_main_conf_t;

// A struct for storing all the stats for a single latency record.
typedef struct {
  ngx_atomic_t request_count;
  ngx_atomic_t sum;
  ngx_atomic_t *distribution;
  // The sum of each sample in the distribution squared. Needed for calculating the variance.
  ngx_atomic_t sum_squares;
} latency_stat;

// A struct containing all the latency records to be stored in shared memory
typedef struct {
  latency_stat *request_latency;
  latency_stat *upstream_latency;
  latency_stat *websocket_latency;
} ngx_http_latency_shm_t;


// Get the latency stats stored in shared memory.
ngx_http_latency_shm_t *get_latency_record(ngx_http_request_t *r);

#endif
