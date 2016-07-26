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

SERVICE_NAME=''
SERVICE_VERSION=''

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
$(command basename $0) -n SERVICE_NAME -v SERVICE_VERSION
Options:
    -h
        Shows this message.
    -n ENDPOINTS_SERVICE_NAME
        Required. The name of the Endpoints Service.
        e.g. my-service.my-project-id.appspot.com
    -v ENDPOINTS_SERVICE_VERSION
        Required. The version of the Endpoints Service which is assigned
        when deploying the service API specification.
        e.g. 2016-04-20R662
dd    -
END_USAGE
  exit 1
}
while getopts 'ha:n:N:p:S:s:v:' arg; do
  case ${arg} in
    h) usage;;
    n) SERVICE_NAME="${OPTARG}";;
    v) SERVICE_VERSION="${OPTARG}";;
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

# fetch Service Configuration from Service Management if the service name and
# service version are provided.
if [[ -n "${SERVICE_NAME}" && -n "${SERVICE_VERSION}" ]]; then
  /usr/sbin/fetch_service_config.sh \
    -s "${SERVICE_NAME}" -v "${SERVICE_VERSION}" || exit $?
fi

/usr/sbin/nginx -p /usr -c /etc/nginx/nginx.conf
