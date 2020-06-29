
#ifndef _NGX_HTTP_LATENCY_RECORDER_H_INCLUDED_
#define _NGX_HTTP_LATENCY_RECORDER_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct { 
  ngx_flag_t enable;
} ngx_http_latency_conf_t;

/* record latency */
void *ngx_http_latency_create_loc_conf(ngx_conf_t *cf);
char *ngx_http_latency_merge_loc_conf(ngx_conf_t *cf, void *parent, void* child);
ngx_int_t ngx_http_latency_init(ngx_conf_t *cf);

#endif
