#!/bin/bash

BUILD_DATE_FILE=/config/build_date.txt
METADATA_FILE=/tmp/metadata.txt
OPENTELEMETRY_CONFIG_FILE=/opentelemetry_config.yaml
METADATA_URL="http://metadata.google.internal/computeMetadata/v1/instance"

get_metadata () {
  local METADATA_PATH="${1}"
  local REPLACE_NAME="${2}"

  local STATUS_CODE=$(curl --write-out %{http_code} --silent --output "${METADATA_FILE}" -H "Metadata-Flavor: Google" "${METADATA_URL}/${METADATA_PATH}")
  if [[ "${STATUS_CODE}" -eq 200 ]]; then
    sed -i "s!@${REPLACE_NAME}@!$(cat ${METADATA_FILE})!" "${OPENTELEMETRY_CONFIG_FILE}"
  else
    sed -i "s/@${REPLACE_NAME}@/unknown/" "${OPENTELEMETRY_CONFIG_FILE}"
  fi
}

if [[ -f "${BUILD_DATE_FILE}" ]]; then
  sed -i "s/@BUILD_DATE@/$(cat ${BUILD_DATE_FILE})/" "${OPENTELEMETRY_CONFIG_FILE}"
else
  sed -i "s/@BUILD_DATE@/unknown/" "${OPENTELEMETRY_CONFIG_FILE}"
fi

get_metadata "image" "IMAGE_NAME"
get_metadata "attributes/gae_backend_version" "VERSION"
get_metadata "attributes/gae_backend_name" "SERVICE"
get_metadata "name" "INSTANCE"

STATUS_CODE=$(curl --write-out %{http_code} --silent --output "${METADATA_FILE}" -H "Metadata-Flavor: Google" "${METADATA_URL}/zone")
if [[ "${STATUS_CODE}" -eq 200 ]]; then
  sed -i 's!.*zones/!!' "${METADATA_FILE}"
  sed -i 's/-.$//' "${METADATA_FILE}"
  sed -i "s!@REGION@!$(cat ${METADATA_FILE})!" "${OPENTELEMETRY_CONFIG_FILE}"
else
  sed -i "s/@REGION@/unknown/" "${OPENTELEMETRY_CONFIG_FILE}"
fi

/opentelemetry_collector --config="${OPENTELEMETRY_CONFIG_FILE}"
