#!/bin/bash
# Copyright 2015 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

CERT_DIR=/etc/ssl/localcerts
KEY_FILE=${CERT_DIR}/lb.key
CSR_FILE=${CERT_DIR}/lb.csr
CRT_FILE=${CERT_DIR}/lb.crt

ENDPOINTS_SERVICE_NAME=''
ENDPOINTS_SERVICE_VERSION=''
ENDPOINTS_ROLLOUT_STRATEGY=''

usage () {
  cat << END_USAGE
Usage: $(command basename $0) [options]
Examples:
(1) Starts nginx with no Endpoints service name and service version. In this
mode either Endpoints features are disabled via nginx.conf or the service
configuration is already provided on disk:
$(command basename $0)
(2) Starts nginx with a custom Endpoints service name and service version, and
have nginx obtain the service configuration:
$(command basename $0) -n ENDPOINTS_SERVICE_NAME -v ENDPOINTS_SERVICE_VERSION
(3) Starts nginx with a custom Endpoints service name and managed rollout
strategy
$(command basename $0) -n ENDPOINTS_SERVICE_NAME -r managed
Options:
    -h
        Shows this message.
    -n ENDPOINTS_SERVICE_NAME
        Required. The name of the Endpoints Service.
        e.g. my-service.my-project-id.appspot.com
    -v ENDPOINTS_SERVICE_VERSION
        Optional. Required when rollout_strategy is "fixed". Specify the service
        config to use when ESP starts.  ESP will download the service config
        with the config id. Forbidden when rollout_strategy is "managed".
        e.g. 2016-04-20R662
    -r ROLLOUT_STRATEGY
        Optional. Specify how ESP will update its service config. The value
        should be either "fixed" or "manage". If it is "fixed", the ESP will
        keep using the service config when it starts.  If it is "managed",
        ESP will constantly check the latest rollout, use the service configs
        specified in the latest rollout. If it is not specified, "fixed"
        would be chosen by default.
        e.g. fixed
    -
END_USAGE
  exit 1
}
while getopts 'ha:n:N:p:S:s:v:r:' arg; do
  case ${arg} in
    h) usage;;
    n) ENDPOINTS_SERVICE_NAME="${OPTARG}";;
    v) ENDPOINTS_SERVICE_VERSION="${OPTARG}";;
    r) ENDPOINTS_ROLLOUT_STRATEGY="${OPTARG}";;
    ?) usage;;
  esac
done

mkdir -p ${CERT_DIR}
if [[ ! -f "${KEY_FILE}" ]]; then
  rm -f ${CSR_FILE} ${CRT_FILE}
  openssl genrsa -out ${KEY_FILE} 2048
  openssl req -new -key ${KEY_FILE} -out ${CSR_FILE} -subj "/"
  openssl x509 -req -in ${CSR_FILE} -signkey ${KEY_FILE} -out ${CRT_FILE}
fi

if [[ -n ${GAE_EXTRA_NGINX_CONFS} ]]; then
  for conf in ${GAE_EXTRA_NGINX_CONFS}; do
    if [[ -f /var/lib/nginx/optional/${conf} ]]; then
      cp /var/lib/nginx/optional/${conf}  /var/lib/nginx/extra
    fi
  done
fi

# Start crond so that log rotation works.
/usr/sbin/service cron restart

# use the override nginx.conf if there is one.
if [[ -f "${CONF_FILE}" ]]; then
  cp "${CONF_FILE}" /etc/nginx/nginx.conf
fi

# Start nginx
if [[ -z "${ENDPOINTS_SERVICE_NAME}" && \
      -z "${ENDPOINTS_ROLLOUT_STRATEGY}" && \
      -z "${ENDPOINTS_SERVICE_VERSION}" ]]; then
  /usr/sbin/nginx -p /usr -c /etc/nginx/nginx.conf
else
  if [[ "${ENDPOINTS_ROLLOUT_STRATEGY}"  && \
        "${ENDPOINTS_ROLLOUT_STRATEGY}" != "fixed" && \
        "${ENDPOINTS_ROLLOUT_STRATEGY}" != "managed" ]]; then
    echo "Error: rollout strategy option should be either fixed or managed"
    usage
  fi

  if [[ "${ENDPOINTS_ROLLOUT_STRATEGY}" != "managed" && \
        "${ENDPOINTS_SERVICE_VERSION}" == "" ]]; then
    echo "Error: version must be specified for fixed rollout strategy"
    usage
  fi

  if [[ "${ENDPOINTS_ROLLOUT_STRATEGY}" == "managed" && \
        "${ENDPOINTS_SERVICE_VERSION}" ]]; then
    echo "Error: version should not be specified for managed rollout strategy"
    usage
  fi

  # Building nginx startup command
  cmd="/usr/sbin/start_esp"
  cmd+=" -n /etc/nginx/nginx.conf"
  cmd+=" -s \"${ENDPOINTS_SERVICE_NAME}\""
  if [[ "${ENDPOINTS_SERVICE_VERSION}" ]]; then
    cmd+=" -v \"${ENDPOINTS_SERVICE_VERSION}\""
  fi
  if [[ "${ENDPOINTS_ROLLOUT_STRATEGY}" ]]; then
    cmd+=" --rollout_strategy \"${ENDPOINTS_ROLLOUT_STRATEGY}\""
  fi

  # Start nginx
  eval $cmd
fi
