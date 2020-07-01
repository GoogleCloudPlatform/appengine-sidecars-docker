
#include "ngx_http_latency_recorder.h"
#include "ngx_http_latency_storage.h"
#include "ngx_http_latency_stub_status_module.h"

static ngx_int_t ngx_http_record_latency(ngx_http_request_t *r);
static ngx_int_t get_latency_index(ngx_int_t max_exponent, ngx_int_t *latency_bucket_bounds, ngx_int_t latency);
static void update_latency_record(latency_stat *record, ngx_int_t latency, ngx_http_latency_main_conf_t *main_conf);

void *ngx_http_latency_create_loc_conf(ngx_conf_t *cf)
{
  ngx_http_latency_conf_t *conf;

  conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_latency_conf_t));
  if (conf == NULL) {
    return NULL;
  }

  conf->enable = NGX_CONF_UNSET;
  ngx_log_stderr(0, "reached latency filter create");

  return conf;
}

char *ngx_http_latency_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
  ngx_log_stderr(0, "reached latency filter merge");
  ngx_http_latency_conf_t *prev = parent;
  ngx_http_latency_conf_t *conf = child;

  ngx_conf_merge_value(conf->enable, prev->enable, 0);

  return NGX_CONF_OK;
}

ngx_int_t ngx_http_latency_init(ngx_conf_t *cf)
{
  ngx_http_core_main_conf_t  *core_main_config;
  ngx_http_handler_pt *handler;

  ngx_log_stderr(0, "reached latency filter init");
  //ngx_http_next_header_filter = ngx_http_top_header_filter;
  //ngx_http_top_header_filter = ngx_http_latency_header_filter;

  core_main_config = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
  handler = ngx_array_push(&core_main_config->phases[NGX_HTTP_LOG_PHASE].handlers);
  *handler = ngx_http_record_latency;

  ngx_log_stderr(0, "reached end of latency filter init");
  return NGX_OK;
}

static ngx_int_t ngx_http_record_latency(ngx_http_request_t *r)
{
  ngx_time_t *now;
  ngx_msec_int_t request_time;
  ngx_http_latency_conf_t *conf;
  ngx_http_latency_main_conf_t *main_conf;
  ngx_http_latency_shm_t *latency_record;

  ngx_log_stderr(0, "reached latency filter");
  conf = ngx_http_get_module_loc_conf(r, ngx_http_latency_stub_status_module);

  if (!conf->enable) {
    return NGX_DECLINED;
    //return ngx_http_next_header_filter(r);
  }

  main_conf = ngx_http_get_module_main_conf(r, ngx_http_latency_stub_status_module);
  if (main_conf == NULL) {
    ngx_log_stderr(0, "error: no main config");
  }
  if (main_conf->latency_bucket_bounds == NULL) {
    ngx_log_stderr(0, "error no latency bounds");
  }
  ngx_log_stderr(0, "reached latency filter after enabled check");
  ngx_log_stderr(0, "max_exponent %d", main_conf->max_exponent);

  ngx_time_update();
  now = ngx_timeofday();
  request_time = (ngx_int_t)((now->sec - r->start_sec) * 1000 + now->msec - r->start_msec);
  request_time = ngx_max(request_time, 0);
  ngx_log_stderr(0, "request time: %d %d %d", request_time, now->sec, r->start_sec);

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

  ngx_log_stderr(0, "upstream request time: %d", upstream_time);
  update_latency_record(latency_record->upstream_latency, upstream_time, main_conf);

  //return ngx_http_next_header_filter(r);
  return NGX_OK;
}

static void update_latency_record(latency_stat *record, ngx_int_t latency, ngx_http_latency_main_conf_t *main_conf) {
  ngx_int_t distribution_index;

  ngx_atomic_fetch_add(&(record->request_count), 1);
  ngx_atomic_fetch_add(&(record->sum), latency);
  ngx_atomic_fetch_add(&(record->sum_squares), latency * latency);

  distribution_index = get_latency_index(main_conf->max_exponent, main_conf->latency_bucket_bounds, latency);
  ngx_atomic_fetch_add(&(record->distribution[distribution_index]), 1);
}

static ngx_int_t get_latency_index(ngx_int_t max_exponent, ngx_int_t *latency_bucket_bounds, ngx_int_t latency) {
  for (ngx_int_t i = 0; i <= max_exponent; i++) {
    if (latency < latency_bucket_bounds[i]) {
      return i;
    }
  }

  // return the index for the overflow bucket.
  return max_exponent + 1;
}

