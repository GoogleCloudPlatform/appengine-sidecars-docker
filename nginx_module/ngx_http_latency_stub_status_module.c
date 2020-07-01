#include "ngx_http_latency_recorder.h"
#include "ngx_http_latency_status_handler.h"
#include "ngx_http_latency_storage.h"
#include "ngx_http_latency_stub_status_module.h"

static latency_stat *create_latency_record(ngx_slab_pool_t *shpool);

static ngx_command_t ngx_http_latency_stub_status_commands[] = {
  {
    ngx_string("latency_stub_status"),
    NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS|NGX_CONF_TAKE1,
    ngx_http_latency_stub_status,
    0,
    0,
    NULL
  },
  {
    ngx_string("record_latency"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
    ngx_conf_set_flag_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_latency_conf_t, enable),
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

static ngx_http_module_t ngx_http_latency_stub_status_module_ctx = {
  NULL,  /* preconfiguration */
  ngx_http_latency_init,                                        /* postconfiguration */
  ngx_http_latency_create_main_conf,                                        /* create main configuration */
  NULL,                                        /* init main configuration */
  NULL,                                        /* create server configuration */
  NULL,                                        /* merge server configuration */
  ngx_http_latency_create_loc_conf,                                        /* create location configuration */
  ngx_http_latency_merge_loc_conf,                                        /* merge location configuration */
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

static ngx_int_t shm_max_exponent;

ngx_int_t ngx_http_latency_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data){
  ngx_slab_pool_t *shpool;
  ngx_http_latency_shm_t *record_set;

  ngx_log_stderr(0, "reached shm init");
  ngx_log_stderr(0, "shm_max_exponent %d", shm_max_exponent);

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

static latency_stat *create_latency_record(ngx_slab_pool_t *shpool) {
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
    return NULL;
  }

  return record;
}
static ngx_int_t ngx_parse_int(ngx_conf_t* cf, ngx_int_t arg_index, ngx_int_t* num) {
  ngx_str_t *value;

  value = cf->args->elts;
  *num = ngx_atoi(value[arg_index].data, value[arg_index].len);;
  if (*num == NGX_ERROR) {
    return NGX_ERROR;
  }

  return NGX_OK;
}


char* ngx_http_latency(ngx_conf_t* cf, ngx_command_t* cmd, void* conf){
  ngx_log_stderr(0, "reached directive call");
  ngx_http_latency_main_conf_t* main_conf;
  ngx_int_t base, scale_factor, max_exponent;
  ngx_int_t parse_result;

  main_conf = (ngx_http_latency_main_conf_t*) conf;

  parse_result = ngx_parse_int(cf, 1, &base);
  if (parse_result != NGX_OK) {
    return "invalid number for latency_status_stub: base";
  }

  parse_result = ngx_parse_int(cf, 2, &scale_factor);
  if (parse_result != NGX_OK) {
    return "invalid number for latency_status_stub: scaled factor";
  }

  parse_result = ngx_parse_int(cf, 3, &max_exponent);
  if (parse_result != NGX_OK) {
    return "invalid number for latency_status_stub: max value";
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
    bucket_bounds[i] = base * power;
    power *= scale_factor;
  }
  main_conf->latency_bucket_bounds = bucket_bounds;

  ngx_shm_zone_t *shm_zone;
  ngx_str_t *shm_name;

  shm_name = ngx_palloc(cf->pool, sizeof *shm_name);
  shm_name->len = sizeof("latency_shared_memory") - 1;
  shm_name->data = (unsigned char *) "shared_memory";
  shm_zone = ngx_shared_memory_add(cf, shm_name, 8 * ngx_pagesize, &ngx_http_latency_stub_status_module);

  if(shm_zone == NULL) {
    return NGX_CONF_ERROR;
  }

  ngx_log_stderr(0, "page size %d", ngx_pagesize);
  shm_zone->init = ngx_http_latency_init_shm_zone;
  main_conf->shm_zone = shm_zone;

  return NGX_CONF_OK;
}


void *ngx_http_latency_create_main_conf(ngx_conf_t* cf){
  ngx_http_latency_main_conf_t* conf;
  ngx_log_stderr(0, "reached create main config");

  conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_latency_main_conf_t));
  if(conf == NULL) {
    return NGX_CONF_ERROR;
  }


  return conf;
}

