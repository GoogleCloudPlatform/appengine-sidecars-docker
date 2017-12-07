#!/usr/bin/python
# Copyright 2017 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""An agent for handling app container updates."""

import argparse
import json
import logging
import os
import time

from google_compute_engine import metadata_watcher

DEFAULT_POLLING_INTERVAL_SEC = 10

def DisableVerification(output_file):
  if os.path.isfile(output_file):
    os.remove(output_file)

def UpdateStateFileFromMetadata(value, output_file):
  """Writes out to the state file on updates.

  Args:
    value: unicode string, the value of the metadata attribute.
    output_file: ascii string, where to write the resulting value to.
  """
  if value:
    logging.info('The latest value is %s.', value)
    try:
      metadata = json.loads(value)
      if 'enabled' in metadata and metadata['enabled']:
        with open(output_file, 'w') as file:
          pass
      else:
        DisableVerification(output_file)
    except ValueError as vErr:
      # if we can't parse the metadata, treat this as "IAP off"
      logging.info('Error decoding metadata.')
      DisableVerification(output_file)
  else:
    # Empty value? Assume off.
    logging.info('Empty metadata value.')
    DisableVerification(output_file)

def Main(argv, watcher=None, loop_watcher=True, os_system=os.system):
  """Runs the watcher.

  Args:
    argv: map => [string, string], Command line arguments
    watcher: MetadataWatcher, used to stub out MetadataWatcher for testing.
    loop_watcher: Boolean, whether or not to loop upon an update.
  """
  logger = logging.getLogger()
  logger.setLevel(logging.INFO)

  # This ensures we have fresh keys at container start. Doing it here because
  # Docker doesn't support multiple CMD/ENTRYPOINT statements in Dockerfiles.
  if (argv.fetch_keys):
    os_system('curl "https://www.gstatic.com/iap/verify/public_key-jwk" > '
              + argv.output_key_file)

  polling_interval = argv.polling_interval

  # Currently, failsafe logic in the nginx module will start failing open if the
  # state file's modification time is more than two mintues in the past, so
  # we enforce that the polling interval is updated sensibly.
  if (polling_interval < 1 or polling_interval > 119):
    polling_interval = DEFAULT_POLLING_INTERVAL_SEC

  watcher = watcher or metadata_watcher.MetadataWatcher()

  while True:
    value = watcher.GetMetadata(
        metadata_key='project/attributes/%s' % argv.iap_metadata_key,
        recursive=False,
        timeout=1)
    UpdateStateFileFromMetadata(value, argv.output_state_file)
    if not loop_watcher:
      break
    time.sleep(polling_interval)

if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Watches for IAP state changes.')
  parser.add_argument('--iap_metadata_key', type=str, required=True,
                      help='Metadata key to be watched for IAP state.')
  parser.add_argument('--output_state_file', type=str, required=True,
                      help='Where to output the state object to.')
  parser.add_argument('--polling_interval', type=int, required=False,
                      help='Seconds between metadata fetch attempts.',
                      default=DEFAULT_POLLING_INTERVAL_SEC)
  parser.add_argument('--fetch_keys', type=bool, required=False,
                      help='Whether to fetch the keys at start up',
                      default=False)
  parser.add_argument('--output_key_file', type=str, required=False,
                      help='Where to output the state object to.')
  args = parser.parse_args()
  Main(args)
