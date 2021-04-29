# OpenTelemetry Collector agent that collects metrics and sends them to Stackdriver

ARG BASE_IMAGE_TAG=latest
ARG GOLANG_TAG=latest


# Tests and builds  opentelemetry_collector.
FROM golang:${GOLANG_TAG} AS builder

RUN mkdir -p /workspace/src/opentelemetry_collector
COPY ./ /workspace/src/opentelemetry_collector
WORKDIR /workspace/src/opentelemetry_collector

ENV GO111MODULE=on
ENV CGO_ENABLED=0
ENV GOOS=linux
ENV GOARCH=amd64

RUN go mod download
RUN go test -v ./...
RUN go build -o opentelemetry_collector .
RUN cp ./opentelemetry_collector /opentelemetry_collector


FROM gcr.io/google-appengine/debian10:${BASE_IMAGE_TAG}

RUN apt-get update && apt-get install -y --no-install-recommends curl

COPY --from=builder opentelemetry_collector /opentelemetry_collector
ADD opentelemetry_config.yaml /opentelemetry_config.yaml
ADD run.sh /run.sh

ENTRYPOINT ["/run.sh"]
