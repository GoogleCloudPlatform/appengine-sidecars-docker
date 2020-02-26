#!/bin/bash

OPENTELEMETRY_CONFIG_FILE=/opentelemetry_config.yaml

set_metadata () {
  local METADATA_VAR="${1}"
  local REPLACE_NAME="${2}"

  if [[ -z "${METADATA_VAR}" ]]; then
    sed -i "s/@${REPLACE_NAME}@/unknown/" "${OPENTELEMETRY_CONFIG_FILE}"
  else
    sed -i "s!@${REPLACE_NAME}@!${METADATA_VAR}!" "${OPENTELEMETRY_CONFIG_FILE}"
  fi
}

set_metadata "${BUILD_DATE}" "BUILD_DATE"
set_metadata "${IMAGE}" "IMAGE_NAME"
set_metadata "${GAE_BACKEND_VERSION}" "VERSION"
set_metadata "${GAE_BACKEND_NAME}" "SERVICE"
set_metadata "${INSTANCE_ID}" "INSTANCE"

if [[ -z "${ZONE}" ]]; then
  sed -i "s/@REGION@/unknown/" "${OPENTELEMETRY_CONFIG_FILE}"
else
  FORMATTED_ZONE=$(echo "${ZONE}" | sed 's!.*zones/!!')
  REGION=$(echo "${FORMATTED_ZONE}" | sed 's/-.$//')
  sed -i "s!@REGION@!${REGION}!" "${OPENTELEMETRY_CONFIG_FILE}"
fi

/opentelemetry_collector --config="${OPENTELEMETRY_CONFIG_FILE}"
