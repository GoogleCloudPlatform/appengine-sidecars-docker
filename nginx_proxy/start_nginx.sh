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

if [[ -n ${GAE_EXTRA_NGINX_CONFS} ]]; then
  for conf in ${GAE_EXTRA_NGINX_CONFS}; do
    if [[ -f /var/lib/nginx/optional/${conf} ]]; then
      cp /var/lib/nginx/optional/${conf}  /var/lib/nginx/extra
    fi
  done
fi

mkdir -p ${CERT_DIR}
if [[ ! -f ${KEY_FILE} ]]; then
  rm -f ${CSR_FILE} ${CRT_FILE}
  openssl genrsa -out ${KEY_FILE} 2048
  openssl req -new -key ${KEY_FILE} -out ${CSR_FILE} -subj "/"
  openssl x509 -req -in ${CSR_FILE} -signkey ${KEY_FILE} -out ${CRT_FILE}
fi

if [[ -f ${CONF_FILE} ]]; then
  cp ${CONF_FILE} /etc/nginx/nginx.conf
fi

/usr/sbin/nginx

