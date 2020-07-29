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

#include "ngx_http_latency_status_handler.h"
#include "ngx_http_latency_storage.h"
#include "ngx_http_latency_stub_status_module.h"


// Write a single latency record to the output buffer in json format.
static void write_distribution_content(ngx_buf_t *buffer, char name[], ngx_int_t len_dist, latency_stat *latency_record) {
  u_char const dist_array_start[] = "    \"distribution\": [";
  u_char const json_sub_var_end[] = "  },\n";

  buffer->last = ngx_sprintf(buffer->last, "  \"%s\":{\n", name);

  buffer->last = ngx_sprintf(buffer->last, "    \"latency_sum\": %uA,\n", latency_record->sum);
  buffer->last = ngx_sprintf(buffer->last, "    \"request_count\": %uA,\n", latency_record->request_count);
  buffer->last = ngx_sprintf(buffer->last, "    \"sum_squares\": %uA,\n", latency_record->sum_squares);
  buffer->last = ngx_cpymem(buffer->last, dist_array_start, sizeof(dist_array_start) - 1);
  for (int i = 0; i < len_dist - 1; i++) {
    buffer->last = ngx_sprintf(buffer->last, "%uA, ", latency_record->distribution[i]);
  }
  buffer->last = ngx_sprintf(buffer->last, "%uA],\n", latency_record->distribution[len_dist]);
  buffer->last = ngx_cpymem(buffer->last, json_sub_var_end, sizeof(json_sub_var_end) - 1);
}


// An http request handler that returns a status page including latency
// distribution stats. The stats are in json format.
ngx_int_t ngx_http_latency_stub_status_handler(ngx_http_request_t *r)
{
  if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
    return NGX_HTTP_NOT_ALLOWED;
  }

  ngx_int_t request_result = ngx_http_discard_request_body(r);

  if (request_result != NGX_OK) {
    return request_result;
  }

  r->headers_out.content_type_len = sizeof("application/json") - 1;
  ngx_str_set(&r->headers_out.content_type, "application/json");
  r->headers_out.content_type_lowcase = NULL;

  if (r->method == NGX_HTTP_HEAD) {
    r->headers_out.status = NGX_HTTP_OK;

    request_result = ngx_http_send_header(r);

    if (request_result == NGX_ERROR || request_result > NGX_OK || r->header_only) {
      return request_result;
    }
  }

  ngx_http_latency_main_conf_t *main_conf = ngx_http_get_module_main_conf(
      r, ngx_http_latency_stub_status_module);
  ngx_int_t dist_len = main_conf->max_exponent + 2;

  u_char const json_start[] = "{\n";
  u_char const json_end[] = "}\n";
  u_char const json_var_start[] = "  \"";
  u_char const json_sub_var_start[] = "    \"";
  u_char const json_var_transition[] = "\": ";
  u_char const json_sub_var_transition[] = "\":{\n";
  u_char const json_var_end[] = ",\n";
  u_char const json_sub_var_end[] = "  }\n";
  u_char const json_array_start[] = "\": [";
  u_char const json_array_sep[] = ", ";
  u_char const json_array_end[] = "],\n";

  size_t latency_stat_size = sizeof(json_var_start) + sizeof(json_sub_var_transition) + sizeof(json_sub_var_start) * 4
      + sizeof(json_var_transition) * 3 + sizeof(json_var_end) * 3
      + sizeof(json_array_start) + sizeof(json_array_sep) * (dist_len - 1) + sizeof(json_array_end)
      + sizeof(json_sub_var_end)
      + sizeof("request_count")
      + sizeof("latency_sum")
      + sizeof("sum_squares")
      + sizeof("distribution")
      + dist_len * NGX_ATOMIC_T_LEN;

  size_t output_size = sizeof(json_start)
      + sizeof("accepted_connections")
      + sizeof("handled_connections")
      + sizeof("active_connections")
      + sizeof("requests")
      + sizeof("reading_connections")
      + sizeof("writing_connections")
      + sizeof("waiting_connections")
      + 7 + (sizeof(json_var_start) + sizeof(json_var_transition) + sizeof(json_var_end))
      + NGX_ATOMIC_T_LEN * 8
      + sizeof("latency_bucket_bounds")
      + sizeof(json_array_start) + sizeof(json_array_sep) * (dist_len - 1) + sizeof(json_array_end)
      + dist_len * NGX_ATOMIC_T_LEN
      + sizeof("request_latency")
      + sizeof("upstream_latency")
      + sizeof("websocket_latency")
      + 3 * latency_stat_size
      + sizeof(json_end);

  ngx_buf_t *buffer = ngx_create_temp_buf(r->pool, output_size);
  if (buffer == NULL) {
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }

  ngx_chain_t out;
  out.buf = buffer;
  out.next = NULL;

  ngx_atomic_int_t accepted = 0;
  ngx_atomic_int_t handled = 0;
  ngx_atomic_int_t active = 0;
  ngx_atomic_int_t requests = 0;
  ngx_atomic_int_t reading = 0;
  ngx_atomic_int_t writing = 0;
  ngx_atomic_int_t waiting = 0;
#if (NGX_STAT_STUB)
  accepted = *ngx_stat_accepted;
  handled = *ngx_stat_handled;
  active = *ngx_stat_active;
  requests = *ngx_stat_requests;
  reading = *ngx_stat_reading;
  writing = *ngx_stat_writing;
  waiting = *ngx_stat_waiting;
#endif

  ngx_http_latency_shm_t *latency_record;
  latency_record = get_latency_record(r);

  u_char const bucket_array_start[] = "  \"latency_bucket_bounds\": [0,";

  buffer->last = ngx_cpymem(buffer->last, json_start, sizeof(json_start) - 1);
  buffer->last = ngx_sprintf(buffer->last, "  \"accepted_connections\": %uA,\n", accepted);
  buffer->last = ngx_sprintf(buffer->last, "  \"handled_connections\": %uA,\n", handled);
  buffer->last = ngx_sprintf(buffer->last, "  \"active_connections\": %uA,\n", active);
  buffer->last = ngx_sprintf(buffer->last, "  \"requests\": %uA,\n", requests);
  buffer->last = ngx_sprintf(buffer->last, "  \"reading_connections\": %uA,\n", reading);
  buffer->last = ngx_sprintf(buffer->last, "  \"writing_connections\": %uA,\n", writing);
  buffer->last = ngx_sprintf(buffer->last, "  \"waiting_connections\": %uA,\n", waiting);

  buffer->last = ngx_cpymem(buffer->last, bucket_array_start, sizeof(bucket_array_start) - 1);
  for (int i = 0; i < dist_len - 2; i++) {
    buffer->last = ngx_sprintf(buffer->last, " %A,", main_conf->latency_bucket_bounds[i]);
  }
  buffer->last = ngx_sprintf(buffer->last, " %A],\n", main_conf->latency_bucket_bounds[dist_len - 2]);

  write_distribution_content(buffer, "request_latency", dist_len, latency_record->request_latency);
  write_distribution_content(buffer, "upstream_latency", dist_len, latency_record->upstream_latency);
  write_distribution_content(buffer, "websocket_latency", dist_len, latency_record->websocket_latency);

  buffer->last = ngx_cpymem(buffer->last, json_end, sizeof(json_end) - 1);

  r->headers_out.status = NGX_HTTP_OK;
  r->headers_out.content_length_n = buffer->last - buffer->pos;

  buffer->last_buf = (r == r->main) ? 1 : 0;
  buffer->last_in_chain = 1;

  request_result = ngx_http_send_header(r);

  if (request_result == NGX_ERROR || request_result > NGX_OK || r->header_only) {
    return request_result;
  }

  return ngx_http_output_filter(r, &out);
}


char* ngx_http_latency_stub_status(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_loc_conf_t *core_loc_config = ngx_http_conf_get_module_loc_conf(
      cf, ngx_http_core_module);
  core_loc_config->handler = ngx_http_latency_stub_status_handler;

  ngx_http_latency_conf_t *location_config = (ngx_http_latency_conf_t*)conf;
  location_config->status_page_enabled = 1;

  return NGX_CONF_OK;
}

