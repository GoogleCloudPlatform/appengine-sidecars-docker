
#ifndef _NGX_HTTP_LATENCY_STORAGE_H_INCLUDED_
#define _NGX_HTTP_LATENCY_STORAGE_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
  ngx_shm_zone_t *shm_zone;
  ngx_int_t base;
  ngx_int_t scale_factor;
  ngx_int_t max_exponent;
  ngx_int_t *latency_bucket_bounds;
} ngx_http_latency_main_conf_t;

typedef struct {
  ngx_atomic_t latency_sum_ms;
  ngx_atomic_t request_count;
  ngx_atomic_t upstream_latency_sum_ms;
  ngx_atomic_t upstream_request_count;
  ngx_atomic_t *latency_distribution;
} ngx_http_latency_shm_t;


/* shared utils */
ngx_http_latency_shm_t *get_latency_record(ngx_http_request_t *r);

#endif
