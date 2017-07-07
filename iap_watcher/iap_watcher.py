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
from functools import partial
import json
import logging
import os
import signal

from google_compute_engine import metadata_watcher


def _ExitWithExceptionHandle(signum, frame):
  logging.error('Timeout when retrieving metadata. Retrying...')


def UpdateStateFileFromMetadataHandler(value, output_file):
  """Writes out to the state file on updates.

  Args:
    value: unicode string, the value of the metadata attribute.
    output_file: ascii string, where to write the resulting value to.
  """
  if value:
    logging.info('The latest value is %s.', value)
    metadata = json.loads(value)
    if metadata['enabled']:
      with open(output_file, 'w') as file:
        pass
    elif os.path.isfile(output_file):
      os.remove(output_file)
  else:
    logging.info('Retry due to empty value.')


def Main(argv, watcher=None, loop_watcher=True):
  """Runs the watcher.

  Args:
    argv: map => [string, string], Command line arguments
    watcher: MetadataWatcher, used to stub out MetadataWatcher for testing.
    loop_watcher: Boolean, whether or not to loop upon an update.
  """
  logger = logging.getLogger()
  logger.setLevel(logging.INFO)

  timeout = argv.timeout or 600
  signal.signal(signal.SIGALRM, _ExitWithExceptionHandle)
  signal.alarm(timeout)

  watcher = watcher or metadata_watcher.MetadataWatcher()

  while True:
    watcher.WatchMetadata(
      partial(
        UpdateStateFileFromMetadataHandler,
        output_file=argv.output_state_file),
      metadata_key='instance/attributes/%s' % argv.iap_metadata_key,
      recursive=False,
      timeout=timeout)
    if loop_watcher:
      break

if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Watches for IAP state changes.')
  parser.add_argument('--iap_metadata_key', type=str, required=True,
                      help='Metadata key to be watched for IAP state.')
  parser.add_argument('--output_state_file', type=str, required=True,
                      help='Where to output the state object to.')
  parser.add_argument('--timeout', type=int, required=False,
                      help='Number of seconds to watch.')
  args = parser.parse_args()
  Main(args)
