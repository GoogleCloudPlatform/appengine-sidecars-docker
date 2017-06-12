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
import signal
import sys
from functools import partial
from google_compute_engine import metadata_watcher


def _ExitWithExceptionHandle(signum, frame):
  logging.error('Timeout when retrieving metadata.')
  sys.exit(1)


def _RetryIfValueIsEmptyHandler(value, output_file):
  """Exits with 0 if the given value is not empty.

  Args:
    value: unicode string, the value of the metadata attribute.
    output_file: ascii string, where to write the resulting value to.
  """
  if value:
    logging.info('The latest value is %s.', value)
    f = open(output_file, 'w')
    f.write(value)
    f.close()
    # Cancel the alarm.
    signal.alarm(0)
    sys.exit(0)
  else:
    logging.info('Retry due to empty value.')


def Main(args, watcher=None):
  """Runs the watcher.

  Args:
    args: map => [string, string], Command line arguments
    watcher: MetadataWatcher, used to stub out MetadataWatcher for testing.
  """
  logger = logging.getLogger()
  logger.setLevel(logging.INFO)

  timeout = args.timeout or 600
  signal.signal(signal.SIGALRM, _ExitWithExceptionHandle)
  signal.alarm(timeout)

  watcher = watcher or metadata_watcher.MetadataWatcher()
  watcher.WatchMetadata(partial(_RetryIfValueIsEmptyHandler, output_file=args.output_state_file),
                        metadata_key='instance/attributes/%s' % args.key,
                        recursive=False,
                        timeout=timeout)

if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Watches for IAP state changes.')
  parser.add_argument(
      '--key', type=str, required=True, help='Metadata key to be watched for IAP state.')
  parser.add_argument(
      '--output_state_file', type=str, required=True, help='Where to output the state object to.')
  parser.add_argument(
      '--timeout', type=int, required=False, help='Number of seconds to watch.')
  args = parser.parse_args()
  Main(args)

