
#include "ngx_http_latency_status_handler.h"
#include "ngx_http_latency_storage.h"
#include "ngx_http_latency_stub_status_module.h"

//static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static void write_distribution_content(ngx_buf_t *buffer, char name[], ngx_int_t len_dist, ngx_atomic_t *latency_distribution);

ngx_int_t ngx_http_latency_stub_status_handler(ngx_http_request_t *r)
{
  size_t output_size;
  ngx_int_t request_result;
  ngx_buf_t *buffer;
  ngx_chain_t out;
  ngx_http_latency_main_conf_t *main_conf;
  ngx_int_t dist_len;
  ngx_atomic_int_t accepted, handled, active, requests, reading, writing, waiting;
  ngx_atomic_int_t latency_sum, latency_requests, upstream_latency_sum, upstream_latency_requests;

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
  u_char const json_var_transition[] = "\": \"";
  u_char const json_var_end[] = "\",\n";
  u_char const json_array_start[] = "\": [";
  u_char const json_array_sep[] = ", ";
  u_char const json_array_end[] = "],\n";

  output_size = sizeof(json_start)
      + sizeof("accepted_connections")
      + sizeof("handled_connections")
      + sizeof("active_connections")
      + sizeof("requests")
      + sizeof("reading_connections")
      + sizeof("writing_connections")
      + sizeof("waiting_connections")
      + sizeof("latency_sum")
      + sizeof("latency_requests")
      + sizeof("upstream_latency_sum")
      + sizeof("upstream_latency_requests")
      + sizeof("version")
      + sizeof(json_var_start) * 12 + sizeof(json_var_transition) * 12 + sizeof(json_var_end) * 12
      + NGX_ATOMIC_T_LEN * 12
      + sizeof("latency_distribution")
      + sizeof("upstream_latency_distribution")
      + 2 * (sizeof(json_var_start) + sizeof(json_array_start) + sizeof(json_array_sep) * (dist_len - 1) + sizeof(json_array_end))
      + 2 * dist_len * NGX_ATOMIC_T_LEN
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

  latency_sum = latency_record->latency_sum_ms;
  latency_requests = latency_record->request_count;
  upstream_latency_sum = latency_record->upstream_latency_sum_ms;
  upstream_latency_requests = latency_record->upstream_request_count;

  //latency_distribution = latency_record->latency_distribution;

  buffer->last = ngx_cpymem(buffer->last, json_start, sizeof(json_start) - 1);
  buffer->last = ngx_sprintf(buffer->last, "  \"accepted_connections\": \"%uA\",\n", accepted);
  buffer->last = ngx_sprintf(buffer->last, "  \"handled_connections\": \"%uA\",\n", handled);
  buffer->last = ngx_sprintf(buffer->last, "  \"active_connections\": \"%uA\",\n", active);
  buffer->last = ngx_sprintf(buffer->last, "  \"requests\": \"%uA\",\n", requests);
  buffer->last = ngx_sprintf(buffer->last, "  \"reading_connections\": \"%uA\",\n", reading);
  buffer->last = ngx_sprintf(buffer->last, "  \"writing_connections\": \"%uA\",\n", writing);
  buffer->last = ngx_sprintf(buffer->last, "  \"waiting_connections\": \"%uA\",\n", waiting);
  buffer->last = ngx_sprintf(buffer->last, "  \"latency_sum\": \"%uA\",\n", latency_sum);
  buffer->last = ngx_sprintf(buffer->last, "  \"latency_requests\": \"%uA\",\n", latency_requests);
  buffer->last = ngx_sprintf(buffer->last, "  \"upstream_latency_sum\": \"%uA\",\n", upstream_latency_sum);
  buffer->last = ngx_sprintf(buffer->last, "  \"upstream_latency_requests\": \"%uA\",\n", upstream_latency_requests);

  write_distribution_content(buffer, "latency_distribution", dist_len, latency_record->latency_distribution);
  write_distribution_content(buffer, "upstream_latency_distribution", dist_len, latency_record->upstream_latency_distribution);

  buffer->last = ngx_sprintf(buffer->last, "  \"version\": \"4\",\n", waiting);
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

static void write_distribution_content(ngx_buf_t *buffer, char name[], ngx_int_t len_dist, ngx_atomic_t *latency_distribution) {

  buffer->last = ngx_sprintf(buffer->last, "  \"%s\": [", name);
  for (int i = 0; i < len_dist - 1; i++) {
    buffer->last = ngx_sprintf(buffer->last, "%uA, ", latency_distribution[i]);
  }
  buffer->last = ngx_sprintf(buffer->last, "%uA],\n", latency_distribution[len_dist]);
}
