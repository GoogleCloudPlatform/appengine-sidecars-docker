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

#include "ngx_http_latency_recorder.h"
#include "ngx_http_latency_status_handler.h"
#include "ngx_http_latency_storage.h"
#include "ngx_http_latency_stub_status_module.h"

// A variable used to get the max_exponent from the main config in the
// shared memory initialization
static ngx_int_t shm_max_exponent;


// Initialize a single set of latency metrics.
latency_stat *create_latency_record(ngx_slab_pool_t *shpool) {
  latency_stat *record;

  record = ngx_slab_alloc(shpool, sizeof(latency_stat));
  if (record == NULL) {
    return record;
  }

  record->request_count = 0;
  record->sum = 0;
  record->sum_squares = 0;

  record->distribution = ngx_slab_calloc(shpool, sizeof(ngx_atomic_t) * (shm_max_exponent + 2));
  if (record->distribution == NULL) {
    ngx_slab_free(shpool, record);
    return NULL;
  }

  return record;
}


// Initialize the shared memory used to store the latency statistics.
ngx_int_t ngx_http_latency_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data){
  ngx_slab_pool_t *shpool;
  ngx_http_latency_shm_t *record_set;

  if (data) {
    shm_zone->data = data;
    return NGX_OK;
  }

  shpool = (ngx_slab_pool_t *)shm_zone->shm.addr;
  record_set = ngx_slab_alloc(shpool, sizeof(ngx_http_latency_shm_t));

  record_set->request_latency = create_latency_record(shpool);
  record_set->upstream_latency = create_latency_record(shpool);
  record_set->websocket_latency = create_latency_record(shpool);

  if (record_set->request_latency == NULL ||
      record_set->upstream_latency == NULL ||
      record_set->websocket_latency == NULL) {
    return NGX_ERROR;
  }

  shm_zone->data = record_set;
  return NGX_OK;
}


// Parse an integer input from a config. arg_index is the index of the directive
// argument to be parsed, where the name of the directive is argument 0.
ngx_int_t ngx_parse_int(ngx_conf_t* cf, ngx_int_t arg_index) {
  ngx_str_t *value;

  value = cf->args->elts;
  return ngx_atoi(value[arg_index].data, value[arg_index].len);
}


// Parse the config for the store_latency directive.
// It expects 3 arguments, base, scaled_factor, and max_exponent,  defining the buckets
// for the latency distributions that will be stored.
// There will be max_exponent + 2 buckets with the boundaries
// [0, scaled_factor * base ^ i) for i = 0
// [scaled_factor * base ^ (i - 1), scaled_factor * base ^ i ) for 0 < i <= max_exponent
// [scaled_factor * base ^ (i - 1), infinity) for i = max_exponent + 1
char* ngx_http_latency(ngx_conf_t* cf, ngx_command_t* cmd, void* conf){
  ngx_http_latency_main_conf_t* main_conf;
  ngx_int_t base, scale_factor, max_exponent;
  ngx_str_t *config_value;

  config_value = cf->args->elts;
  main_conf = (ngx_http_latency_main_conf_t*) conf;

  base = ngx_parse_int(cf, 1);
  if (base == NGX_ERROR) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid value for latency_status_stub: base \"%V\"", &config_value[1]);
    return NGX_CONF_ERROR;
  }

  scale_factor = ngx_parse_int(cf, 2);
  if (scale_factor == NGX_ERROR) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid value for latency_status_stub: scaled_factor \"%V\"", &config_value[2]);
    return NGX_CONF_ERROR;
  }

  max_exponent = ngx_parse_int(cf, 3);
  if (max_exponent == NGX_ERROR) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid value for latency_status_stub: max_value \"%V\"", &config_value[3]);
    return NGX_CONF_ERROR;
  }

  if (base <= 1) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid value for latency_status_stub: base %d. It must be greater than 1", base);
    return NGX_CONF_ERROR;
  }
  if (scale_factor <= 0) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid value for latency_status_sub: scaled_factor %d. It must be greater than 0", scale_factor);
    return NGX_CONF_ERROR;
  }
  if (max_exponent < 1) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid value for latency_status_sub: max_exponent %d. It must be at least 1", max_exponent);
    return NGX_CONF_ERROR;
  }

  main_conf->base = base;
  main_conf->scale_factor = scale_factor;
  main_conf->max_exponent = max_exponent;
  shm_max_exponent = max_exponent;

  ngx_int_t* bucket_bounds;
  ngx_int_t power = 1;
  bucket_bounds = ngx_palloc(cf->pool, sizeof(ngx_int_t) * (max_exponent + 1));
  if (bucket_bounds == NULL) {
    return NGX_CONF_ERROR;
  }
  for (ngx_int_t i = 0; i <= max_exponent; i++) {
    bucket_bounds[i] = scale_factor * power;
    power *= base;
  }
  main_conf->latency_bucket_bounds = bucket_bounds;

  ngx_shm_zone_t *shm_zone;
  ngx_str_t *shm_name;

  shm_name = ngx_palloc(cf->pool, sizeof(ngx_str_t));
  shm_name->len = sizeof("latency_shared_memory") - 1;
  shm_name->data = (unsigned char *) "latency_shared_memory";
  shm_zone = ngx_shared_memory_add(cf, shm_name, 8 * ngx_pagesize, &ngx_http_latency_stub_status_module);

  if(shm_zone == NULL) {
    return NGX_CONF_ERROR;
  }

  shm_zone->init = ngx_http_latency_init_shm_zone;
  main_conf->shm_zone = shm_zone;

  main_conf->enabled = 1;
  return NGX_CONF_OK;
}


// Create a new ngx_http_latency_main_conf_t.
void *ngx_http_latency_create_main_conf(ngx_conf_t* cf){
  ngx_http_latency_main_conf_t* conf;

  conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_latency_main_conf_t));
  if(conf == NULL) {
    return NGX_CONF_ERROR;
  }

  conf->enabled = NGX_CONF_UNSET;
  return conf;
}


// Create a new ngx_http_latency_conf_t.
void *ngx_http_latency_create_loc_conf(ngx_conf_t *cf)
{
  ngx_http_latency_conf_t *conf;

  conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_latency_conf_t));
  if (conf == NULL) {
    return NULL;
  }

  conf->receiver_enabled = NGX_CONF_UNSET;
  conf->status_page_enabled = NGX_CONF_UNSET;
  return conf;
}


// Merge ngx_http_latency_conf_t configs in case they have been set in multiple
// locations.
char *ngx_http_latency_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
  ngx_http_latency_conf_t *prev = parent;
  ngx_http_latency_conf_t *conf = child;
  ngx_http_latency_main_conf_t *main_conf;

  ngx_conf_merge_value(conf->receiver_enabled, prev->receiver_enabled, 0);
  ngx_conf_merge_value(conf->status_page_enabled, prev->status_page_enabled, 0);
  main_conf = ngx_http_conf_get_module_main_conf(cf, ngx_http_latency_stub_status_module);

  if (!(main_conf->enabled) || main_conf->enabled == NGX_CONF_UNSET) {

    if (conf->receiver_enabled) {
      ngx_conf_log_error(NGX_LOG_ERR, cf, 0,
                         "cannot enable latency recording for a location when latency storage is not enabled in the main conf.");
      conf->receiver_enabled = 0;
    }
    if (conf->status_page_enabled) {
      ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                         "cannot enable the latency status page for a location when the latency storage is not enabled in the main conf.");
      return NGX_CONF_ERROR;
    }
  }

  return NGX_CONF_OK;
}


// The commands define directives that can be used in the module.
static ngx_command_t ngx_http_latency_stub_status_commands[] = {
  {
    ngx_string("latency_stub_status"),
    NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS|NGX_CONF_TAKE1,
    ngx_http_latency_stub_status,
    NGX_HTTP_LOC_CONF_OFFSET,
    0,
    NULL
  },
  {
    ngx_string("record_latency"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_latency_conf_t, receiver_enabled),
    NULL
  },
  {
    ngx_string("store_latency"),
    NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE3,
    ngx_http_latency,
    NGX_HTTP_MAIN_CONF_OFFSET,
    0,
    NULL
  },
  ngx_null_command
};


// The context sets initialization and configuration callbacks.
static ngx_http_module_t ngx_http_latency_stub_status_module_ctx = {
  NULL,                                        /* preconfiguration */
  ngx_http_latency_init,                       /* postconfiguration */
  ngx_http_latency_create_main_conf,           /* create main configuration */
  NULL,                                        /* init main configuration */
  NULL,                                        /* create server configuration */
  NULL,                                        /* merge server configuration */
  ngx_http_latency_create_loc_conf,            /* create location configuration */
  ngx_http_latency_merge_loc_conf,             /* merge location configuration */
};


// The module definition.
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
