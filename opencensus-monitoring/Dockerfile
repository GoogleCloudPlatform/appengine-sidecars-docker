# OpenTelemetry Collector agent that collects metrics and sends them to Stackdriver

ARG BASE_IMAGE_TAG=latest
FROM gcr.io/google-appengine/debian9:${BASE_IMAGE_TAG}

ADD opentelemetry-collector /opentelemetry-collector
ADD opentelemetry_config.yaml /opentelemetry_config.yaml

ENTRYPOINT ["/opentelemetry-collector", "--config=/opentelemetry_config.yaml"]
