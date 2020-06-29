#include "ngx_http_latency_storage.h"
#include "ngx_http_latency_stub_status_module.h"

ngx_http_latency_shm_t *get_latency_record(ngx_http_request_t *r) {
  ngx_http_latency_main_conf_t *main_conf;
  ngx_shm_zone_t *shm_zone;

  main_conf = ngx_http_get_module_main_conf(r, ngx_http_latency_stub_status_module);

  if(main_conf->shm_zone == NULL) {
    return NULL;
  }

  shm_zone = main_conf->shm_zone;
  return (ngx_http_latency_shm_t *)shm_zone->data;
}
