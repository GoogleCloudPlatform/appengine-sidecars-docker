#!/bin/bash

if [[ -n ${GAE_EXTRA_NGINX_CONFS} ]]; then
  for conf in ${GAE_EXTRA_NGINX_CONFS}; do
    if [[ -f /var/lib/nginx/optional/${conf} ]]; then
      cp /var/lib/nginx/optional/${conf}  /var/lib/nginx/extra
    fi
  done
fi

if [[ -f ${CONF_FILE} ]]; then
  cp ${CONF_FILE} /etc/nginx/nginx.conf
fi

/usr/sbin/nginx

