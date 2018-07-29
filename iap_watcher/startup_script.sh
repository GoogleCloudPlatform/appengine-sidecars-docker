#!/bin/bash
set -e

# Initial key fetch and cron job setup
KEYS_ENDPOINT=https://www.gstatic.com/iap/verify/public_key-jwk
OUTPUT_KEY_FILE=/iap_watcher/iap_verify_keys.txt
curl "${KEYS_ENDPOINT}" > ${OUTPUT_KEY_FILE}
echo "$(($RANDOM%60)) * * * * curl \"${KEYS_ENDPOINT}\" > ${OUTPUT_KEY_FILE}" \
  > .tmp_cron
crontab .tmp_cron
rm .tmp_cron
service cron start

# Start the IAP watcher
exec ./iap_watcher.py \
  --iap_metadata_key=AEF_IAP_state \
  --output_state_file=/iap_watcher/iap_state \
  --output_key_file=${OUTPUT_KEY_FILE}

