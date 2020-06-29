#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
  ngx_flag_t enable;
} ngx_http_latency_conf_t;

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

static ngx_int_t shm_max_exponent;

/* server latency stats */
static char *ngx_http_latency_stub_status(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_latency_stub_status_handler(ngx_http_request_t *r);

/* record latency */
static void *ngx_http_latency_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_latency_merge_loc_conf(ngx_conf_t *cf, void *parent, void* child);
static ngx_int_t ngx_http_latency_init(ngx_conf_t *cf);

/* init latency storage */
static char *ngx_http_latency(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void *ngx_http_latency_create_main_conf(ngx_conf_t *cf);
static ngx_int_t ngx_http_latency_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data);

/* shared utils */
static ngx_http_latency_shm_t *get_latency_record(ngx_http_request_t *r);

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

static ngx_int_t ngx_http_latency_stub_status_handler(ngx_http_request_t *r)
{
  size_t output_size;
  ngx_int_t request_result;
  ngx_buf_t *buffer;
  ngx_chain_t out;
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
      + sizeof(json_var_start) + sizeof(json_array_start) + sizeof(json_array_sep) * 19 + sizeof(json_array_end)
      + NGX_ATOMIC_T_LEN * 20
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

  buffer->last = ngx_cpymem(buffer->last, "  \"latency_distribution\": [", sizeof("  \"latency_distribution\": [") -1);
  for (int i = 0; i < 19; i++) {
    buffer->last = ngx_sprintf(buffer->last, "%uA, ", latency_record->latency_distribution[i]);
  }
  buffer->last = ngx_sprintf(buffer->last, "%uA],\n", latency_record->latency_distribution[19]);

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

static char* ngx_http_latency_stub_status(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_log_stderr(0, "reached latency status stub init");

  ngx_http_core_loc_conf_t *core_loc_config;

  core_loc_config = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
  core_loc_config->handler = ngx_http_latency_stub_status_handler;

  return NGX_CONF_OK;
}

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;

static ngx_int_t ngx_http_latency_header_filter(ngx_http_request_t *r)
{
  ngx_time_t *now;
  ngx_msec_int_t request_time;
  ngx_http_latency_conf_t *conf;

  ngx_log_stderr(0, "reached latency filter");
  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "catch request body filter before enable check");
  conf = ngx_http_get_module_loc_conf(r, ngx_http_latency_stub_status_module);

  if (!conf->enable) {
    return ngx_http_next_header_filter(r);
  }

  ngx_log_stderr(0, "reached latency filter after enabled check");
  ngx_time_update();
  now = ngx_timeofday();
  request_time = (ngx_int_t)((now->sec - r->start_sec) * 1000 + now->msec - r->start_msec);
  request_time = ngx_max(request_time, 0);
  ngx_log_stderr(0, "request time: %d %d %d", request_time, now->sec, r->start_sec);

  ngx_http_upstream_state_t *state;
  ngx_http_latency_shm_t *latency_record;
  latency_record = get_latency_record(r);

  if(latency_record == NULL) {
    return NGX_DECLINED;
  }

  ngx_atomic_fetch_add(&(latency_record->request_count), 1);
  ngx_atomic_fetch_add(&(latency_record->latency_sum_ms), request_time);
  ngx_int_t not_found = 1;
  int i = 0;
  while (not_found && i < 19) {
    if (request_time < (i + 1) * 100) {
      ngx_atomic_fetch_add(&(latency_record->latency_distribution[i]), 1);
      not_found = 0;
      ngx_log_stderr(0, "latency in bucket %d", i);
    }
    i++;
  }
  if (not_found) {
    ngx_log_stderr(0, "latency in bucket 19");
    ngx_atomic_fetch_add(&(latency_record->latency_distribution[19]), 1);
  }

  if (r->upstream_states == NULL || r->upstream_states->nelts == 0) {
    return NGX_OK;
  }

  ngx_msec_int_t upstream_time = 0;
  state = r->upstream_states->elts;
  for (ngx_uint_t i = 0; i < r->upstream_states->nelts; i++) {
    upstream_time += state[i].response_time;
  }

  ngx_log_stderr(0, "upstream request time: %d", upstream_time);


  ngx_atomic_fetch_add(&(latency_record->upstream_request_count), 1);
  ngx_atomic_fetch_add(&(latency_record->upstream_latency_sum_ms), upstream_time);

  //return ngx_http_next_header_filter(r);
  return NGX_OK;
}

static ngx_http_latency_shm_t *get_latency_record(ngx_http_request_t *r) {
  ngx_http_latency_main_conf_t *main_conf;
  ngx_shm_zone_t *shm_zone;

  main_conf = ngx_http_get_module_main_conf(r, ngx_http_latency_stub_status_module);

  if(main_conf->shm_zone == NULL) {
    return NULL;
  }
  shm_zone = main_conf->shm_zone;
  return (ngx_http_latency_shm_t *)shm_zone->data;
}

static void *ngx_http_latency_create_loc_conf(ngx_conf_t *cf)
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

static char *ngx_http_latency_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
  ngx_log_stderr(0, "reached latency filter merge");
  ngx_http_latency_conf_t *prev = parent;
  ngx_http_latency_conf_t *conf = child;

  ngx_conf_merge_value(conf->enable, prev->enable, 0);

  return NGX_CONF_OK;
}

static ngx_int_t ngx_http_latency_init(ngx_conf_t *cf)
{
  ngx_http_core_main_conf_t  *core_main_config;
  ngx_http_handler_pt *handler;

  ngx_log_stderr(0, "reached latency filter init");
  //ngx_http_next_header_filter = ngx_http_top_header_filter;
  //ngx_http_top_header_filter = ngx_http_latency_header_filter;

  core_main_config = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
  handler = ngx_array_push(&core_main_config->phases[NGX_HTTP_LOG_PHASE].handlers);
  *handler = ngx_http_latency_header_filter;

  ngx_log_stderr(0, "reached end of latency filter init");
  return NGX_OK;
}

static ngx_int_t ngx_http_latency_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data){
  ngx_slab_pool_t *shpool;
  ngx_http_latency_shm_t *latency_record;

  ngx_log_stderr(0, "reached shm init");
  ngx_log_stderr(0, "shm_max_exponent %d", shm_max_exponent);

  if (data) {
    shm_zone->data = data;
    return NGX_OK;
  }

  shpool = (ngx_slab_pool_t *)shm_zone->shm.addr;
  latency_record = ngx_slab_alloc(shpool, sizeof(ngx_http_latency_shm_t));
  latency_record->latency_sum_ms = 0;
  latency_record->request_count = 0;
  latency_record->upstream_latency_sum_ms = 0;
  latency_record->upstream_request_count = 0;

  latency_record->latency_distribution = ngx_slab_calloc(shpool, sizeof(ngx_atomic_t) * (shm_max_exponent + 1));
  if (latency_record->latency_distribution == NULL) {
    ngx_log_stderr(0, "array alloc error");
  }
  for (int i = 0; i < shm_max_exponent + 1; i++) {
    latency_record->latency_distribution[i] = 0;
  }

  shm_zone->data = latency_record;
  return NGX_OK;
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

static char* ngx_http_latency(ngx_conf_t* cf, ngx_command_t* cmd, void* conf){
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

static void *ngx_http_latency_create_main_conf(ngx_conf_t* cf){
  ngx_http_latency_main_conf_t* conf;
  ngx_log_stderr(0, "reached create main config");

  conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_latency_main_conf_t));
  if(conf == NULL) {
    return NGX_CONF_ERROR;
  }
  return conf;
}

