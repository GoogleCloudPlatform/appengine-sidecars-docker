#!/bin/bash

BUILD_DATE_FILE=/config/build_date.txt
IMAGE_NAME_FILE=/tmp/image_name.txt
OPENTELEMETRY_CONFIG_FILE=/opentelemetry_config.yaml
METADATA_URL="http://metadata.google.internal/computeMetadata/v1/instance/image"

if [[ -f "${BUILD_DATE_FILE}" ]]; then
  sed -i "s/@BUILD_DATE@/$(cat ${BUILD_DATE_FILE})/" "${OPENTELEMETRY_CONFIG_FILE}"
else
  sed -i "s/@BUILD_DATE@/unknown/" "${OPENTELEMETRY_CONFIG_FILE}"
fi

STATUS_CODE=$(curl --write-out %{http_code} --silent --output "${IMAGE_NAME_FILE}" -H "Metadata-Flavor: Google" "{$METADATA_URL}")
if [[ "${STATUS_CODE}" -eq 200 ]]; then
  sed -i "s!@IMAGE_NAME@!$(cat ${IMAGE_NAME_FILE})!" "${OPENTELEMETRY_CONFIG_FILE}"
else
  sed -i "s/@IMAGE_NAME@/unknown/" "${OPENTELEMETRY_CONFIG_FILE}"
fi

/opentelemetry-collector --config="${OPENTELEMETRY_CONFIG_FILE}"
