#!/bin/bash

BUILD_DATE_FILE=/config/build_date.txt
OPENTELEMETRY_CONFIG_FILE=/opentelemetry_config.yaml

if [[ -f "${BUILD_DATE_FILE}" ]]; then
  sed -i "s/@BUILD_DATE@/$(cat ${BUILD_DATE_FILE})/" "${OPENTELEMETRY_CONFIG_FILE}"
else
  sed -i "s/@BUILD_DATE@/unknown/" "${OPENTELEMETRY_CONFIG_FILE}"
fi

/opentelemetry-collector --config="${OPENTELEMETRY_CONFIG_FILE}"
