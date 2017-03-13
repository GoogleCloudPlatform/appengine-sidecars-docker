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

"""A library for watching changes in the metadata server."""

import argparse
import httplib
import logging
import sys
import time
import urllib
import urllib2

ATTRIBUTES_BASE_URL = 'http://metadata/computeMetadata/v1/instance/attributes/'


class MetadataWatcher(object):
  """Watches for some attribute changes in metadata."""

  def __init__(self, etag=None, sleep_time_seconds=3):
    """Constructor.

    Args:
      etag: string, the etag for identifying metadata version.
      sleep_time_seconds: int, interval time in seconds between two GET
          requests.
    """
    self.etag = etag
    self.sleep_time_seconds = sleep_time_seconds

  def _SendRequest(self, metadata_key, timeout_seconds=None):
    """Performs a GET request to the metadata service to fetch all attributes.

    Args:
      metadata_key: string, the metadata key to watch for changes.
      timeout_seconds: int, timeout in seconds for watching metadata change.

    Returns:
      HTTP response from the GET request.

    Raises:
      urlerror.HTTPError: raises when the GET request fails.
    """
    headers = {'Metadata-Flavor': 'Google'}
    while True:
      try:
        params = {
            'last_etag': self.etag,
            'timeout_sec': timeout_seconds,
            'wait_for_change': True,
        }
        params_string = urllib.urlencode(params)
        url = '%s%s?%s' % (ATTRIBUTES_BASE_URL, metadata_key, params_string)
        request = urllib2.Request(url, headers=headers)
        response = urllib2.urlopen(request)
      except urllib2.HTTPError as error:
        if (self._ShouldContinueSendingRequest(error) and
            not self._IsTimeout(timeout_seconds)):
          logging.warning('Error when retrieving metadata: %s.', error)
          time.sleep(self.sleep_time_seconds)
          continue
        else:
          raise error
      else:
        if response.getcode() == httplib.OK:
          return response
        else:
          code = response.getcode()
          message = httplib.responses.get(code)
          raise urllib2.HTTPError(
              url, code, message, response.headers, response)

  def _ShouldContinueSendingRequest(self, error):
    """Returns True if sending GET request should be continued.

    If the given error caught when performing GET request is metadata service
    unavailable, or URL not found, or request timeout, it should continue
    sending GET request.

    Args:
      error: urllib2.HTTPError.

    Returns:
      bool, whether should continue sending GET request.
    """
    return (error.getcode() == httplib.SERVICE_UNAVAILABLE or
            error.getcode() == httplib.NOT_FOUND or
            error.getcode() == httplib.REQUEST_TIMEOUT)

  def _GetAttributeUpdate(self, metadata_key, timeout_seconds=None):
    """Requests the value of the given metadata key.

    It keeps sending requests for the value of the given metadata key if there
    is an update and the value is not empty.

    Args:
      metadata_key: string, the metadata key to watch for changes.
      timeout_seconds: int, timeout in seconds for watching metadata change.

    Returns:
      string, the value of the given metadata key.
    """
    while True:
      response = self._SendRequest(metadata_key, timeout_seconds)
      etag = response.headers.get('etag', self.etag)
      etag_updated = self.etag != etag
      self.etag = etag
      value = response.read()
      if self._IsTimeout(timeout_seconds):
        return value
      else:
        if etag_updated:
          if value:
            return value
          else:
            logging.info('Retry due to empty value of "%s".', metadata_key)
            continue
        else:
          logging.info('Retry due to no update of value of "%s".', metadata_key)
          continue

  def _IsTimeout(self, timeout_seconds):
    """Returns true if the process to watch metadata change is timeout."""
    if not timeout_seconds:
      return False
    return time.time() >= self.start_time + timeout_seconds

  def GetAttribute(self, metadata_key, timeout_seconds=None):
    """Fetches the value of the given metadata key.

    Checks the value of the given metadata key has been updated and is not
    empty.

    Args:
      metadata_key: string, the metadata key to watch for changes.
      timeout_seconds: int, timeout in seconds for watching metadata change.

    Returns:
      string, the value of the given metadata key.
    """
    try:
      self.start_time = time.time()
      value = self._GetAttributeUpdate(metadata_key, timeout_seconds)
      if value:
        logging.info('The latest value of "%s" is %s.', metadata_key, value)
        return value
      else:
        logging.warning('The value of "%s" is empty.', metadata_key)
        sys.exit(1)
    except Exception as e:
      logging.error('Error when retrieving metadata. %s.', e)
      sys.exit(1)


if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Set up metadata watcher.')
  parser.add_argument(
      '--key', type=str, required=True, help='metadata key to be watched.')
  parser.add_argument(
      '--timeout', type=int, required=False, help='number of seconds to watch.')
  parser.add_argument(
      '--etag', type=str, required=False, help='etag in HTTP headers.')
  args = parser.parse_args()

  logger = logging.getLogger()
  logger.setLevel(logging.INFO)

  watcher = MetadataWatcher(etag=args.etag)
  watcher.GetAttribute(args.key, timeout_seconds=args.timeout)
