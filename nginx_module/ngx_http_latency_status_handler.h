
#ifndef _NGX_HTTP_LATENCY_STATUS_HANDLER_H_INCLUDED_
#define _NGX_HTTP_LATENCY_STATUS_HANDLER_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

/* server latency stats */
char *ngx_http_latency_stub_status(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
ngx_int_t ngx_http_latency_stub_status_handler(ngx_http_request_t *r);

#endif
