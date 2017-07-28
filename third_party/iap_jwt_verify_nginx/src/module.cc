// Copyright (C) 2002-2016 Igor Sysoev
// Copyright (C) 2011-2016 Nginx, Inc.
// Copyright (C) 2017 Google Inc.
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

#include "src/module.h"

#include <mutex>

#include "src/iap_jwt_verification.h"

namespace google {
namespace cloud {
namespace iap {

// Only trust IAP state for a maximum of five minutes.
constexpr unsigned int MAX_STATE_CACHE_TIME_SEC = 300;  // 5 minutes

// While the theoretically time to cache the JWT verification keys is 1 day,
// refresh every 12 hours to be on the safe side.
constexpr unsigned int MAX_KEY_CACHE_TIME_SEC = 43200;  // 12 hours

// Used for fail open logic--if the mtime of the IAP state file is more than
// this many seconds in the past, then we assume that metadata fetching is
// not working and fail open preemptively.
constexpr unsigned int MAX_STATE_STALENESS_SEC = 120;

// nginx lowercases headers prior to hashing, so using a lowercase value here
constexpr char IAP_JWT_HEADER_NAME[] = "x-goog-iap-jwt-assertion";

// A constant used to construct the expected JWT audience.
constexpr char IAP_JWT_AUD_PROJECTS[] = "/projects/";

// A constant used to construct the expected JWT audience.
constexpr char IAP_JWT_AUD_APPS[] = "/apps/";

// The name of the nginx variable used to store the IAP JWT verification
// module's action.
constexpr char IAP_JWT_ACTION_VAR_NAME[] = "iap_jwt_action";

// Index of the action variable in a request object.
ngx_int_t action_var_idx;

// Used to synchronize state updates.
ngx_atomic_t iap_state_lock;

// Used to synchronize key updates and access to the main conf's shared key
// pointer.
std::mutex key_mutex;

const ngx_str_t ACTION_NOOP_OFF = ngx_string("noop_off");
const ngx_str_t ACTION_NOOP_IAP_OFF = ngx_string("noop_iap_off");
const ngx_str_t ACTION_ALLOW = ngx_string("allow");
const ngx_str_t ACTION_DENY = ngx_string("deny");

void set_action_value(ngx_http_variable_value_t *v, const ngx_str_t &action) {
  v->len = action.len;
  v->data = action.data;
  v->valid = 1;
  v->no_cacheable = 0;
  v->not_found = 0;
}

ngx_int_t ngx_http_iap_jwt_action_var_get(
    ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data) {
  ngx_http_variable_value_t *real_val = r->variables + action_var_idx;
  if (real_val->data == nullptr || real_val->len == 0) {
    set_action_value(real_val, ACTION_NOOP_OFF);
  }

  *v = *real_val;
  return NGX_OK;
}

// Extract the X-Goog-Iap-Jwt-Assertion header. Unfortunately, nginx internals
// limit us to a linear search (without reaching in and modifying the guts,
// versus just compiling in a new module).
ngx_str_t *extract_iap_jwt_header(ngx_http_request_t *r) {
  ngx_list_part_t *part;
  ngx_table_elt_t *h;
  ngx_uint_t i;

  part = &r->headers_in.headers.part;
  h = reinterpret_cast<ngx_table_elt_t *>(part->elts);

  static const ngx_str_t HEADER_NAME = ngx_string(IAP_JWT_HEADER_NAME);
  for (i = 0;; i++) {
    if (i >= part->nelts) {
      if (part->next == nullptr) {
        break;
      }

      part = part->next;
      h = reinterpret_cast<ngx_table_elt_t *>(part->elts);
      i = 0;
    }

    if (HEADER_NAME.len != h[i].key.len
        || ngx_strcasecmp(HEADER_NAME.data, h[i].key.data) != 0) {
      continue;
    }

    return &h[i].value;
  }

  return nullptr;
}

// IAP JWT verification handler--core business logic lives here and in functions
// called by this handler.
ngx_int_t ngx_http_iap_jwt_verification_handler(ngx_http_request_t *r) {
  ngx_iap_jwt_verify_loc_conf_t *loc_conf =
      reinterpret_cast<ngx_iap_jwt_verify_loc_conf_t *>(
          ngx_http_get_module_loc_conf(r, ngx_iap_jwt_verify_module));

  ngx_http_variable_value_t *action_var_val = r->variables + action_var_idx;

  // If IAP JWT verification is off for this location, return NGX_OK to allow
  // access.
  if (loc_conf->iap_jwt_verify == 0) {
    set_action_value(action_var_val, ACTION_NOOP_OFF);
    return NGX_OK;
  }

  ngx_iap_jwt_verify_main_conf_t *main_conf =
      reinterpret_cast<ngx_iap_jwt_verify_main_conf_t *>(
          ngx_http_get_module_main_conf(r, ngx_iap_jwt_verify_module));

  time_t now = ngx_time();
  if (now >= main_conf->last_iap_state_check.load()
                + main_conf->iap_state_cache_time_sec) {
    if (ngx_trylock(&iap_state_lock, 1)) {
      // Check the condition again to account for the case where another thread
      // acquired and released the lock between when we checked the condition
      // and when we acquired the lock.
      if (now >= main_conf->last_iap_state_check.load()
              + main_conf->iap_state_cache_time_sec) {
        ngx_fd_t fd = ngx_open_file(
            main_conf->iap_state_file.data,
            NGX_FILE_RDONLY,
            NGX_FILE_OPEN,
            0);
        if (fd != NGX_INVALID_FILE) {
          // Existence of file ==> IAP is ON
          main_conf->iap_on = true;

          // Fail-open logic
          ngx_file_info_t info;
          ngx_fd_info(fd, &info);
          time_t mtime = ngx_file_mtime(&info);

          if (now >= mtime + MAX_STATE_STALENESS_SEC) {
            main_conf->fail_open_because_state_stale = true;
          } else {
            main_conf->fail_open_because_state_stale = false;
          }

          if (ngx_close_file(fd) == NGX_FILE_ERROR) {
            // TODO: log the error for diagnostics.
            // Preferably without danger of excessive log spam.
          }
        } else {
          main_conf->iap_on = false;
        }

        // Update time of last state check to be now.
        main_conf->last_iap_state_check = now;
      }

      // Release the lock.
      ngx_unlock(&iap_state_lock);
    }

    // If we fail to get the lock, this just means that another thread is doing
    // the state update already. We choose to simply risk using a stale
    // main_conf->iap_on value to proceed smoothly with no risk of getting
    // stuck. Since there's up to a 5 minute window already where incorrect
    // behavior can occur (based on the cache interval), this isn't a big deal.
  }

  if (!main_conf->iap_on) {
    set_action_value(action_var_val, ACTION_NOOP_IAP_OFF);
    return NGX_OK;
  }

  if (main_conf->fail_open_because_state_stale) {
    // TODO: log fail-open error for processing.
    set_action_value(action_var_val, ACTION_ALLOW);
    return NGX_OK;
  }

  std::unique_lock<std::mutex> lock(key_mutex);
  std::shared_ptr<iap_key_map_t> key_map = main_conf->key_map;
  if (now >= main_conf->last_key_map_update + main_conf->key_cache_time_sec) {
    std::shared_ptr<iap_key_map_t> new_key_map =
        load_keys(reinterpret_cast<const char *>(main_conf->key_file.data),
                  main_conf->key_file.len);
    if (new_key_map == nullptr) {
      // We failed to update the keys. Thus, we update neither key_map nor
      // the time of last update. This means that the next thread to get the
      // lock will attempt to do the update, and so on, until success is
      // achieved.
      //
      // TODO: log this failure.
      // TODO: is this a performance concern?
    } else {
      // Success; update time of last update and shared_ptr to the key map.
      main_conf->last_key_map_update = now;
      main_conf->key_map = new_key_map;
      key_map = new_key_map;
    }
  }

  lock.unlock();

  if (key_map == nullptr) {
    // This likely means the process just began to take traffic, and another
    // thread has taken over the work of loading the keys. We have three
    // choices:
    //
    // 1) deny the request
    // 2) authorize the request
    // 3) block
    //
    // The choice made here is #2 for 30 seconds, thereafter #1. Justification:
    // the security risk of allowing requests through for 30 seconds is
    // negligible, especially since we potentially tolerate up to 5 minutes of
    // no enforcement after an IAP enable. Also, loading the keys into memory
    // should take much fewer than 30 seconds, so the real gap should be even
    // smaller. Continuing to allow requests through indefinitely would not be
    // acceptable (this would ignore potential issues with key fetching), so
    // after 30 seconds, requests will be denied if key_map remains nullptr.
    if (now > main_conf->last_key_map_update + 30) {
      set_action_value(action_var_val, ACTION_ALLOW);
      return NGX_OK;
    } else {
      set_action_value(action_var_val, ACTION_DENY);
      return NGX_HTTP_FORBIDDEN;
    }
  }

  ngx_str_t *jwt_value = extract_iap_jwt_header(r);
  if (jwt_value == nullptr) {
    set_action_value(action_var_val, ACTION_DENY);
    return NGX_HTTP_FORBIDDEN;
  }

  if (iap_jwt_is_valid(
          reinterpret_cast<const char *>(jwt_value->data),
          jwt_value->len,
          static_cast<uint64_t>(now),
          reinterpret_cast<const char *>(main_conf->expected_aud.data),
          main_conf->expected_aud.len,
          key_map)) {
    set_action_value(action_var_val, ACTION_ALLOW);
    return NGX_OK;
  }

  set_action_value(action_var_val, ACTION_DENY);
  return NGX_HTTP_FORBIDDEN;
}

// Assemble the expected JWT audience and store it in the main IAP module
// configuration.
ngx_int_t init_expected_aud(ngx_conf_t *cf,
                            ngx_iap_jwt_verify_main_conf_t *iap_main_conf) {
  ngx_str_t project_number = iap_main_conf->project_number;
  if (project_number.data == nullptr || project_number.len == 0) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "IAP JWT Verification error: no project number.");
    return NGX_ERROR;
  }

  ngx_str_t app_id = iap_main_conf->app_id;
  if (app_id.data == nullptr || app_id.len == 0) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "IAP JWT Verification error: no app id.");
    return NGX_ERROR;
  }

  size_t part_lens[4];
  part_lens[0] = strlen(IAP_JWT_AUD_PROJECTS);
  part_lens[1] = project_number.len;
  part_lens[2] = strlen(IAP_JWT_AUD_APPS);
  part_lens[3] = app_id.len;
  size_t expected_aud_len = 0;
  for (int i = 0; i < 4; i++) {
    expected_aud_len += part_lens[i];
  }

  u_char *data =
      reinterpret_cast<u_char *>(ngx_pcalloc(cf->pool, expected_aud_len + 1));
  if (data == nullptr) {
    return NGX_ERROR;
  }

  ngx_memcpy(data, IAP_JWT_AUD_PROJECTS, part_lens[0]);
  size_t offset = part_lens[0];
  ngx_memcpy(data + offset, project_number.data, part_lens[1]);
  offset += part_lens[1];
  ngx_memcpy(data + offset, IAP_JWT_AUD_APPS, part_lens[2]);
  offset += part_lens[2];
  ngx_memcpy(data + offset, app_id.data, part_lens[3]);
  iap_main_conf->expected_aud.data = data;
  iap_main_conf->expected_aud.len = expected_aud_len;
  return NGX_OK;
}

ngx_int_t ngx_iap_jwt_verify_preconfiguration(ngx_conf_t *cf) {
  // Create the variable that will be used to record the action taken by the
  // verification logic.
  static ngx_str_t action_var_name = ngx_string(IAP_JWT_ACTION_VAR_NAME);
  ngx_http_variable_t *action_var = ngx_http_add_variable(
      cf,
      &action_var_name,
      NGX_HTTP_VAR_CHANGEABLE && NGX_HTTP_VAR_NOCACHEABLE);
  action_var->get_handler = ngx_http_iap_jwt_action_var_get;
  action_var_idx = ngx_http_get_variable_index(cf, &action_var_name);
  return NGX_OK;
}

// Postconfiguration--installs the IAP JWT Verification module's handler into
// the list of NGX_HTTP_ACCESS_PHASE handlers and assembles the expected
// audience value from the supplied configuration pieces.
ngx_int_t ngx_iap_jwt_verify_postconfiguration(ngx_conf_t *cf) {
  ngx_iap_jwt_verify_main_conf_t *iap_main_conf =
      reinterpret_cast<ngx_iap_jwt_verify_main_conf_t *>(
          ngx_http_conf_get_module_main_conf(cf, ngx_iap_jwt_verify_module));
  if (!iap_main_conf->module_in_use) {
    // If the config doesn't request IAP JWT verification, there's no point in
    // inserting the handler.
    return NGX_OK;
  }

  ngx_http_core_main_conf_t *cmcf =
      reinterpret_cast<ngx_http_core_main_conf_t *>(
          ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module));
  ngx_http_handler_pt *h = reinterpret_cast<ngx_http_handler_pt *>(
      ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers));
  if (h == nullptr) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "Cannot install IAP JWT Verification handler.");
    return NGX_ERROR;
  }

  *h = ngx_http_iap_jwt_verification_handler;
  return init_expected_aud(cf, iap_main_conf);
}

// Create a new ngx_iap_jwt_verify_main_conf_t
void *ngx_iap_jwt_verify_create_main_conf(ngx_conf_t *cf) {
  ngx_iap_jwt_verify_main_conf_t *conf =
      reinterpret_cast<ngx_iap_jwt_verify_main_conf_t *>(
          ngx_pcalloc(cf->pool, sizeof(ngx_iap_jwt_verify_main_conf_t)));
  if (conf == nullptr) {
    return NGX_CONF_ERROR;
  }

  conf->iap_state_cache_time_sec = NGX_CONF_UNSET;
  conf->key_cache_time_sec = NGX_CONF_UNSET;

  // Assume the module is not used. The location config merging configuration
  // will modify this value if the module is being used.
  conf->module_in_use = false;
  return conf;
}

char *ngx_iap_jwt_verify_init_main_conf(ngx_conf_t *cf, void *conf) {
  ngx_iap_jwt_verify_main_conf_t *main_conf =
      reinterpret_cast<ngx_iap_jwt_verify_main_conf_t *>(conf);

  // Always assume IAP is off initially--state indicator file may not have been
  // created yet.
  main_conf->iap_on = false;

  // Make sure we are not failing open to start with.
  main_conf->fail_open_because_state_stale = false;

  // These should be zero due to the use of ngx_pcalloc above, but since having
  // their initial values be zero is important for correctness, take no chances.
  main_conf->last_iap_state_check = 0;
  main_conf->last_key_map_update = 0;

  if (main_conf->iap_state_cache_time_sec == NGX_CONF_UNSET) {
    main_conf->iap_state_cache_time_sec = MAX_STATE_CACHE_TIME_SEC;
  } else if (
      main_conf->iap_state_cache_time_sec < 0
      || main_conf->iap_state_cache_time_sec > MAX_STATE_CACHE_TIME_SEC) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "Invalid IAP state cache time.");
    return reinterpret_cast<char *>(NGX_CONF_ERROR);
  }

  if (main_conf->key_cache_time_sec == NGX_CONF_UNSET) {
    main_conf->key_cache_time_sec = MAX_KEY_CACHE_TIME_SEC;
  } else if (
      main_conf->key_cache_time_sec < 0
      || main_conf->key_cache_time_sec > MAX_KEY_CACHE_TIME_SEC) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "Invalid key cache time.");
    return reinterpret_cast<char *>(NGX_CONF_ERROR);
  }

  return reinterpret_cast<char *>(NGX_CONF_OK);
}

// Create a new ngx_iap_jwt_verify_loc_conf_t
void *ngx_iap_jwt_verify_create_loc_conf(ngx_conf_t *cf) {
  ngx_iap_jwt_verify_loc_conf_t *conf;
  conf = reinterpret_cast<ngx_iap_jwt_verify_loc_conf_t *>(
      ngx_pcalloc(cf->pool, sizeof(ngx_iap_jwt_verify_loc_conf_t)));
  if (conf == nullptr) {
    return NGX_CONF_ERROR;
  }
  conf->iap_jwt_verify = NGX_CONF_UNSET;
  return conf;
}

// Merge location config
char *ngx_iap_jwt_verify_merge_loc_conf(
    ngx_conf_t *cf, void *parent, void *child) {
  ngx_iap_jwt_verify_loc_conf_t *prev =
      reinterpret_cast<ngx_iap_jwt_verify_loc_conf_t *>(parent);
  ngx_iap_jwt_verify_loc_conf_t *conf =
      reinterpret_cast<ngx_iap_jwt_verify_loc_conf_t *>(child);
  ngx_conf_merge_value(conf->iap_jwt_verify, prev->iap_jwt_verify, 0);

  if (conf->iap_jwt_verify == 1) {
    ngx_iap_jwt_verify_main_conf_t *iap_main_conf =
        reinterpret_cast<ngx_iap_jwt_verify_main_conf_t *>(
            ngx_http_conf_get_module_main_conf(cf, ngx_iap_jwt_verify_module));
    iap_main_conf->module_in_use = true;
  }

  return reinterpret_cast<char *>(NGX_CONF_OK);
}

// The module context conatins initialization and configuration callbacks.
ngx_http_module_t ngx_iap_jwt_verify_module_ctx = {
    // ngx_int_t (*preconfiguration)(ngx_conf_t *cf);
    ngx_iap_jwt_verify_preconfiguration,
    // ngx_int_t (*postconfiguration)(ngx_conf_t *cf);
    ngx_iap_jwt_verify_postconfiguration,
    // void *(*create_main_conf)(ngx_conf_t *cf);
    ngx_iap_jwt_verify_create_main_conf,
    // char *(*init_main_conf)(ngx_conf_t *cf, void *conf);
    ngx_iap_jwt_verify_init_main_conf,
    // void *(*create_srv_conf)(ngx_conf_t *cf);
    nullptr,
    // char *(*merge_srv_conf)(ngx_conf_t *cf, void *prev, void *conf);
    nullptr,
    // void *(*create_loc_conf)(ngx_conf_t *cf);
    ngx_iap_jwt_verify_create_loc_conf,
    // char *(*merge_loc_conf)(ngx_conf_t *cf, void *prev, void *conf);
    ngx_iap_jwt_verify_merge_loc_conf,
};

// The module commands define the directives that can be used with the module.
ngx_command_t ngx_iap_jwt_verify_commands[] = {
  {
    // name
    ngx_string("iap_jwt_verify"),
    // type
    NGX_CONF_TAKE1 | NGX_HTTP_LOC_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_MAIN_CONF,
    // function to convert command info to values in the module configuration
    ngx_conf_set_flag_slot,
    // which config location to save this command into
    NGX_HTTP_LOC_CONF_OFFSET,
    // offset in configuration struct
    0,
    // void *post
    nullptr
  },
  {
    // name
    ngx_string("iap_jwt_verify_project_number"),
    // type
    NGX_CONF_TAKE1 | NGX_HTTP_MAIN_CONF,
    // function to convert command info to values in the module configuration
    ngx_conf_set_str_slot,
    // which config location to save this command into,
    NGX_HTTP_MAIN_CONF_OFFSET,
    // offset in configuration struct
    0,
    // void *post
    nullptr
  },
  {
    // name
    ngx_string("iap_jwt_verify_app_id"),
    // type
    NGX_CONF_TAKE1 | NGX_HTTP_MAIN_CONF,
    // function to convert command info to values in the module configuration
    ngx_conf_set_str_slot,
    // which config location to save this command into,
    NGX_HTTP_MAIN_CONF_OFFSET,
    // offset in configuration struct
    sizeof(ngx_str_t),
    // void *post
    nullptr
  },
  {
    // name
    ngx_string("iap_jwt_verify_key_file"),
    // type
    NGX_CONF_TAKE1 | NGX_HTTP_MAIN_CONF,
    // function to convert command info to values in the module configuration
    ngx_conf_set_str_slot,
    // which config location to save this command into,
    NGX_HTTP_MAIN_CONF_OFFSET,
    // offset in configuration struct
    2 * sizeof(ngx_str_t),
    // void *post
    nullptr
  },
  {
    // name
    ngx_string("iap_jwt_verify_iap_state_file"),
    // type
    NGX_CONF_TAKE1 | NGX_HTTP_MAIN_CONF,
    // function to convert command info to values in the module configuration
    ngx_conf_set_str_slot,
    // which config location to save this command into,
    NGX_HTTP_MAIN_CONF_OFFSET,
    // offset in configuration struct
    3 * sizeof(ngx_str_t),
    // void *post
    nullptr
  },
  {
    // name
    ngx_string("iap_jwt_verify_state_cache_time_sec"),
    // type
    NGX_CONF_TAKE1 | NGX_HTTP_MAIN_CONF,
    // function to convert command info to values in the module configuration
    ngx_conf_set_num_slot,
    // which config location to save this command into,
    NGX_HTTP_MAIN_CONF_OFFSET,
    // offset in configuration struct
    4 * sizeof(ngx_str_t),
    // void *post
    nullptr
  },
  {
    // name
    ngx_string("iap_jwt_verify_key_cache_time_sec"),
    // type
    NGX_CONF_TAKE1 | NGX_HTTP_MAIN_CONF,
    // function to convert command info to values in the module configuration
    ngx_conf_set_num_slot,
    // which config location to save this command into,
    NGX_HTTP_MAIN_CONF_OFFSET,
    // offset in configuration struct
    4 * sizeof(ngx_str_t) + sizeof(ngx_int_t),
    // void *post
    nullptr
  },
  // indicates end of the array
  ngx_null_command
};

}  // namespace iap
}  // namespace cloud
}  // namespace google

// The module definition must be globally scoped.
ngx_module_t ngx_iap_jwt_verify_module = {
  // module version number
  NGX_MODULE_V1,
  // module context
  &::google::cloud::iap::ngx_iap_jwt_verify_module_ctx,
  // commands
  ::google::cloud::iap::ngx_iap_jwt_verify_commands,
  // module type
  NGX_HTTP_MODULE,
  // ngx_int_t (*init_master)(ngx_log_t *log)
  nullptr,
  // ngx_int_t (*init_module)(ngx_cycle_t *cycle);
  nullptr,
  // ngx_int_t (*init_process)(ngx_cycle_t *cycle);
  nullptr,
  // ngx_int_t (*init_thread)(ngx_cycle_t *cycle);
  nullptr,
  // void (*exit_thread)(ngx_cycle_t *cycle);
  nullptr,
  // void (*exit_process)(ngx_cycle_t *cycle);
  nullptr,
  // void (*exit_master)(ngx_cycle_t *cycle);
  nullptr,
  // padding the rest of the ngx_module_t structure
  NGX_MODULE_V1_PADDING
};

