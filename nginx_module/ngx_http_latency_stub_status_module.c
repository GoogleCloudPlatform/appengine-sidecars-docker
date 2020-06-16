#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

static char *ngx_http_latency_stub_status(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_latency_stub_status_handler(ngx_http_request_t *r);

static ngx_command_t ngx_http_latency_stub_status_commands[] = {
  {
    ngx_string("latency_stub_status"),
    NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS|NGX_CONF_TAKE1,
    ngx_http_latency_stub_status,
    0,
    0,
    NULL
  },
  ngx_null_command
};

static ngx_http_module_t ngx_http_latency_stub_status_module_ctx = {
  NULL,  /* preconfiguration */
  NULL,                                        /* postconfiguration */
  NULL,                                        /* create main configuration */
  NULL,                                        /* init main configuration */
  NULL,                                        /* create server configuration */
  NULL,                                        /* merge server configuration */
  NULL,                                        /* create location configuration */
  NULL,                                        /* merge location configuration */
};

ngx_module_t ngx_http_latency_stub_status_module = {
  NGX_MODULE_V1,
  &ngx_http_latency_stub_status_module_ctx,  /* module context */
  ngx_http_latency_stub_status_commands,     /* module directives */
  NGX_HTTP_MODULE,                           /* module type */
  NULL,                                      /* init master */
  NULL,                                      /* init module */
  NULL,                                      /* init process */
  NULL,                                      /* init thread */
  NULL,                                      /* exit thread */
  NULL,                                      /* exit process */
  NULL,                                      /* exit master */
  NGX_MODULE_V1_PADDING
};

static ngx_int_t ngx_http_latency_stub_status_handler(ngx_http_request_t *r)
{
  size_t output_size;
  ngx_int_t request_result;
  ngx_buf_t *buffer;
  ngx_chain_t out;
  ngx_atomic_int_t accepted, handled, active, requests, reading, writing, waiting;

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

  output_size = sizeof("{\n")
      + sizeof("  \"accepted_connections\": \"\",\n") + NGX_ATOMIC_T_LEN
      + sizeof("  \"handled_connections\": \"\",\n") + NGX_ATOMIC_T_LEN
      + sizeof("  \"active_connections\": \"\",\n") + NGX_ATOMIC_T_LEN
      + sizeof("  \"requests\": \"\",\n") + NGX_ATOMIC_T_LEN
      + sizeof("  \"reading_connections\": \"\",\n") + NGX_ATOMIC_T_LEN
      + sizeof("  \"writing_connections\": \"\",\n") + NGX_ATOMIC_T_LEN
      + sizeof("  \"waiting_connections\": \"\",\n") + NGX_ATOMIC_T_LEN
      + sizeof("}\n");

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

  buffer->last = ngx_cpymem(buffer->last, "{\n", sizeof("{\n") - 1);
  buffer->last = ngx_sprintf(buffer->last, "  \"accepted_connections\": \"%uA\",\n", accepted);
  buffer->last = ngx_sprintf(buffer->last, "  \"handled_connections\": \"%uA\",\n", handled);
  buffer->last = ngx_sprintf(buffer->last, "  \"active_connections\": \"%uA\",\n", active);
  buffer->last = ngx_sprintf(buffer->last, "  \"requests\": \"%uA\",\n", requests);
  buffer->last = ngx_sprintf(buffer->last, "  \"reading_connections\": \"%uA\",\n", reading);
  buffer->last = ngx_sprintf(buffer->last, "  \"writing_connections\": \"%uA\",\n", writing);
  buffer->last = ngx_sprintf(buffer->last, "  \"waiting_connections\": \"%uA\",\n", waiting);
  buffer->last = ngx_cpymem(buffer->last, "}\n", sizeof("}\n") - 1);

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

static char* ngx_http_latency_stub_status(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_loc_conf_t *core_loc_config;

  core_loc_config = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
  core_loc_config->handler = ngx_http_latency_stub_status_handler;

  return NGX_CONF_OK;
}
