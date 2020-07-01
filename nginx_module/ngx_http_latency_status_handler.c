
#include "ngx_http_latency_status_handler.h"
#include "ngx_http_latency_storage.h"
#include "ngx_http_latency_stub_status_module.h"

//static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static void write_distribution_content(ngx_buf_t *buffer, char name[], ngx_int_t len_dist, latency_stat *latency_record);

ngx_int_t ngx_http_latency_stub_status_handler(ngx_http_request_t *r)
{
  size_t output_size, latency_stat_size;
  ngx_int_t request_result;
  ngx_buf_t *buffer;
  ngx_chain_t out;
  ngx_http_latency_main_conf_t *main_conf;
  ngx_int_t dist_len;
  ngx_atomic_int_t accepted, handled, active, requests, reading, writing, waiting;

  ngx_log_stderr(0, "reached latency status stub handler\n");

  if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
    return NGX_HTTP_NOT_ALLOWED;
  }

  request_result = ngx_http_discard_request_body(r);

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

  main_conf = ngx_http_get_module_main_conf(r, ngx_http_latency_stub_status_module);
  dist_len = main_conf->max_exponent + 2;

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

  latency_stat_size = sizeof(json_var_start) + sizeof(json_sub_var_transition) + sizeof(json_sub_var_start) * 4
      + sizeof(json_var_transition) * 3 + sizeof(json_var_end) * 3
      + sizeof(json_array_start) + sizeof(json_array_sep) * (dist_len - 1) + sizeof(json_array_end)
      + sizeof(json_sub_var_end)
      + sizeof("request_count")
      + sizeof("latency_sum")
      + sizeof("sum_squares")
      + sizeof("distribution")
      + dist_len * NGX_ATOMIC_T_LEN;

  output_size = sizeof(json_start)
      + sizeof("accepted_connections")
      + sizeof("handled_connections")
      + sizeof("active_connections")
      + sizeof("requests")
      + sizeof("reading_connections")
      + sizeof("writing_connections")
      + sizeof("waiting_connections")
      + 7 + (sizeof(json_var_start) + sizeof(json_var_transition) + sizeof(json_var_end))
      + NGX_ATOMIC_T_LEN * 8
      + sizeof("request_latency")
      + sizeof("upstream_latency")
      + sizeof("websocket_latency")
      + 3 * latency_stat_size
      + sizeof(json_end);

  buffer = ngx_create_temp_buf(r->pool, output_size);
  if (buffer == NULL) {
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }

  out.buf = buffer;
  out.next = NULL;

  accepted = *ngx_stat_accepted;
  handled = *ngx_stat_handled;
  active = *ngx_stat_active;
  requests = *ngx_stat_requests;
  reading = *ngx_stat_reading;
  writing = *ngx_stat_writing;
  waiting = *ngx_stat_waiting;

  ngx_http_latency_shm_t *latency_record;
  latency_record = get_latency_record(r);

  buffer->last = ngx_cpymem(buffer->last, json_start, sizeof(json_start) - 1);
  buffer->last = ngx_sprintf(buffer->last, "  \"accepted_connections\": %uA,\n", accepted);
  buffer->last = ngx_sprintf(buffer->last, "  \"handled_connections\": %uA,\n", handled);
  buffer->last = ngx_sprintf(buffer->last, "  \"active_connections\": %uA,\n", active);
  buffer->last = ngx_sprintf(buffer->last, "  \"requests\": %uA,\n", requests);
  buffer->last = ngx_sprintf(buffer->last, "  \"reading_connections\": %uA,\n", reading);
  buffer->last = ngx_sprintf(buffer->last, "  \"writing_connections\": %uA,\n", writing);
  buffer->last = ngx_sprintf(buffer->last, "  \"waiting_connections\": %uA,\n", waiting);

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

  ngx_time_t *start;
  ngx_time_t *now;
  ngx_int_t request_time;
  start = ngx_timeofday();
  ngx_msleep(1000);

  now = ngx_timeofday();
  request_time = (ngx_int_t)((now->sec - start->sec) * 1000 + now->msec - start->msec);
  request_time = ngx_max(request_time, 0);
  ngx_log_stderr(0, "handler request time: %d %d %d", request_time, now->sec, start->sec);

  return ngx_http_output_filter(r, &out);
}

char* ngx_http_latency_stub_status(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_log_stderr(0, "reached latency status stub init");

  ngx_http_core_loc_conf_t *core_loc_config;

  core_loc_config = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
  core_loc_config->handler = ngx_http_latency_stub_status_handler;

  return NGX_CONF_OK;
}

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
