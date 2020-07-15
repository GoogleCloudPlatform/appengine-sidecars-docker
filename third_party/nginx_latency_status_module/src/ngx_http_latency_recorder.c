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

#include "ngx_http_latency_recorder.h"
#include "ngx_http_latency_storage.h"
#include "ngx_http_latency_stub_status_module.h"


// Get the index of the distribution bucket that a latency fall in.
ngx_int_t get_latency_index(ngx_int_t max_exponent, ngx_int_t *latency_bucket_bounds, ngx_int_t latency) {
  for (ngx_int_t i = 0; i <= max_exponent; i++) {
    if (latency < latency_bucket_bounds[i]) {
      return i;
    }
  }

  // return the index for the overflow bucket.
  return max_exponent + 1;
}


// Update a latency record with a single new latency.
void update_latency_record(latency_stat *record, ngx_int_t latency, ngx_http_latency_main_conf_t *main_conf) {
  ngx_int_t distribution_index;

  ngx_atomic_fetch_add(&record->request_count, 1);
  ngx_atomic_fetch_add(&record->sum, latency);
  ngx_atomic_fetch_add(&record->sum_squares, latency * latency);

  distribution_index = get_latency_index(main_conf->max_exponent, main_conf->latency_bucket_bounds, latency);
  ngx_atomic_fetch_add(&record->distribution[distribution_index], 1);
}


// Record the latency statistics.
ngx_int_t ngx_http_record_latency(ngx_http_request_t *r)
{
  ngx_time_t *now;
  ngx_msec_int_t request_time;
  ngx_http_latency_conf_t *conf;
  ngx_http_latency_main_conf_t *main_conf;
  ngx_http_latency_shm_t *latency_record;

  conf = ngx_http_get_module_loc_conf(r, ngx_http_latency_stub_status_module);

  if (!conf->receiver_enabled) {
    return NGX_DECLINED;
  }

  main_conf = ngx_http_get_module_main_conf(r, ngx_http_latency_stub_status_module);

  // Update the cached current time value.
  // This increases the accuracy of the latency measurment, but may slow down
  // nginx. It may be removed after load testing.
  ngx_time_update();
  now = ngx_timeofday();
  request_time = (ngx_int_t)((now->sec - r->start_sec) * 1000 + now->msec - r->start_msec);
  request_time = ngx_max(request_time, 0);

  latency_record = get_latency_record(r);

  if(latency_record == NULL) {
    return NGX_DECLINED;
  }

  update_latency_record(latency_record->request_latency, request_time, main_conf);

  if (r->headers_in.upgrade) {
    update_latency_record(latency_record->websocket_latency, request_time, main_conf);
  }

  ngx_http_upstream_state_t *state;

  if (r->upstream_states == NULL || r->upstream_states->nelts == 0) {
    return NGX_OK;
  }

  ngx_msec_int_t upstream_time = 0;
  state = r->upstream_states->elts;
  for (ngx_uint_t i = 0; i < r->upstream_states->nelts; i++) {
    upstream_time += state[i].response_time;
  }

  update_latency_record(latency_record->upstream_latency, upstream_time, main_conf);

  return NGX_OK;
}


ngx_int_t ngx_http_latency_init(ngx_conf_t *cf)
{
  ngx_http_core_main_conf_t  *core_main_config;
  ngx_http_handler_pt *handler;

  core_main_config = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
  // Set the handler to run at the very end of the request, after regular
  // filters, so it will record the full latency of the request.
  handler = ngx_array_push(&core_main_config->phases[NGX_HTTP_LOG_PHASE].handlers);
  *handler = ngx_http_record_latency;

  return NGX_OK;
}

