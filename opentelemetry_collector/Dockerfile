# OpenTelemetry Collector agent that collects metrics and sends them to Stackdriver

ARG BASE_IMAGE_TAG=latest
FROM gcr.io/google-appengine/debian9:${BASE_IMAGE_TAG}

RUN apt-get update && apt-get install -y --no-install-recommends curl

ADD opentelemetry_collector /opentelemetry_collector
ADD opentelemetry_config.yaml /opentelemetry_config.yaml
ADD run.sh /run.sh

ENTRYPOINT ["/run.sh"]
