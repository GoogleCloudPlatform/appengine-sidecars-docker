# Nginx Proxy that forwards requests into the appengine application.

ARG BASE_IMAGE_TAG=latest
FROM gcr.io/google_appengine/base:${BASE_IMAGE_TAG}

# Add the Cloud SDK distribution URI as a package source
# Import the Google Cloud public key
# Update and install the Cloud SDK

RUN apt-get -q update && \
    apt-get -y -q upgrade && \
    apt-get install -y -q curl && \
    echo "deb http://packages.cloud.google.com/apt google-cloud-endpoints-jessie main" \
        | tee /etc/apt/sources.list.d/google-cloud-endpoints.list && \
    curl --silent https://packages.cloud.google.com/apt/doc/apt-key.gpg \
        | apt-key add - && \
    apt-get update && \
    apt-get install -y cron python endpoints-runtime && \
    apt-get clean && \
    rm /var/lib/apt/lists/*_*

RUN mkdir -p /var/lib/nginx/optional && \
    mkdir -p /var/lib/nginx/extra && \
    mkdir -p /var/lib/nginx/bin && \
    mkdir -p /var/lib/nginx/inc

ADD static.conf /var/lib/nginx/optional/static.conf
ADD start_nginx.sh /var/lib/nginx/bin/start_nginx.sh
ADD proxy_pass.inc /var/lib/nginx/inc/proxy_pass.inc
ADD nginx.logrotate /etc/logrotate.d/nginx

EXPOSE 8080
EXPOSE 8090

# to run: docker run --link gaeapp:gaeapp -p 8080:8080 --expose 8090

ENTRYPOINT ["/var/lib/nginx/bin/start_nginx.sh"]
