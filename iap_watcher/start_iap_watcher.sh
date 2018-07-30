#!/bin/bash
set -e

# Initial key fetch, done to ensure that keys are available upon startup
KEYS_ENDPOINT=https://www.gstatic.com/iap/verify/public_key-jwk
OUTPUT_KEY_FILE=/iap_watcher/iap_verify_keys.txt
curl "${KEYS_ENDPOINT}" > ${OUTPUT_KEY_FILE}

# Cron job setup. Keys rotate periodically, so it is necessary to fetch them on
# a regular schedule. The maximum theoretical "safe" caching time is 24 hours,
# and the IAP JWT verification nginx module considers them stale after this time
# for safety; here they are fetched every two hours so there are several chances
# to update them before they go stale (e.g. in case the endpoint is down or
# unreachable). The minute at which they are refreshed is randomized to prevent
# sudden load spikes on the key endpoint.
echo "$(($RANDOM%60)) */2 * * * curl \"${KEYS_ENDPOINT}\" > ${OUTPUT_KEY_FILE}"\
  > .tmp_cron
crontab .tmp_cron
rm .tmp_cron
service cron start

# Start the IAP watcher
exec ./iap_watcher.py \
  --iap_metadata_key=AEF_IAP_state \
  --output_state_file=/iap_watcher/iap_state \
  --output_key_file=${OUTPUT_KEY_FILE}
