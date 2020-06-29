
#ifndef NGX_HTTP_LATENCY_STUB_STATUS_MODULE_H_INCLUDED_
#define NGX_HTTP_LATENCY_STUB_STATUS_MODULE_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

/* init latency storage */
char *ngx_http_latency(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
void *ngx_http_latency_create_main_conf(ngx_conf_t *cf);
ngx_int_t ngx_http_latency_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data);

extern ngx_module_t ngx_http_latency_stub_status_module;

#endif
