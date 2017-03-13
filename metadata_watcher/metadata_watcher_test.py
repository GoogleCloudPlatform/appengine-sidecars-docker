"""Tests the tool which watches metadata changes."""

import httplib
import unittest
import urllib
import urllib2
import metadata_watcher
import mock

SLEEP_TIME_SECONDS = 3
TIMEOUT_SECONDS = None
ETAG = '12'
NEW_ETAG = '13'


class VmGaeFullAppContainerTester(unittest.TestCase):
  """Tests metadata_watcher.MetamodelWatcher."""

  @mock.patch('metadata_watcher.logging')
  @mock.patch('metadata_watcher.time')
  def setUp(self, mock_time, mock_logging):
    mock_logging.info.side_effect = None
    mock_logging.warning.side_effect = None
    mock_logging.error.side_effect = None
    mock_time.sleep.side_effect = None
    self.mock_response = mock.Mock()
    self.mock_response.headers.get.return_value = ETAG
    self.mock_response.getcode.return_value = httplib.OK
    self.base_url = 'http://metadata/computeMetadata/v1/instance/attributes/'
    self.expected_headers = {'Metadata-Flavor': 'Google'}

  @mock.patch('urllib2.urlopen')
  @mock.patch('urllib2.Request')
  def testSendRequestShouldReturnResponse(
      self, mock_request, mock_urlopen):
    mock_request.return_value = mock_request
    mock_urlopen.return_value = self.mock_response

    result = metadata_watcher.MetadataWatcher()._SendRequest('key')

    mock_request.assert_called_once_with(self._ExpectedUrl('key'),
                                         headers=self.expected_headers)
    mock_urlopen.assert_called_once_with(mock_request)
    self.assertEqual(result, self.mock_response)

  @mock.patch('urllib2.urlopen')
  @mock.patch('urllib2.Request')
  def testSendRequestShouldSendCorrectRequest(
      self, mock_request, mock_urlopen):
    mock_request.return_value = mock_request
    mock_urlopen.return_value = self.mock_response
    subject = metadata_watcher.MetadataWatcher(etag=ETAG)

    subject._SendRequest('key', timeout_seconds=300)

    mock_request.assert_called_once_with(
        self._ExpectedUrl('key', etag=ETAG, timeout=300),
        headers=self.expected_headers)
    mock_urlopen.assert_called_once_with(mock_request)

  @mock.patch('metadata_watcher.time')
  @mock.patch('urllib2.urlopen')
  @mock.patch('urllib2.Request')
  def testSendRequestShouldRetryWhenSupportedExceptionsCaught(
      self, mock_request, mock_urlopen, mock_time):
    mock_request.return_value = mock_request
    mock_urlopen.side_effect = [
        self._HttpErrorWithCode(code=httplib.REQUEST_TIMEOUT),
        self._HttpErrorWithCode(code=httplib.SERVICE_UNAVAILABLE),
        self._HttpErrorWithCode(code=httplib.NOT_FOUND),
        self.mock_response]

    result = metadata_watcher.MetadataWatcher()._SendRequest('key')

    request_mock_call = mock.call(self._ExpectedUrl('key'),
                                  headers=self.expected_headers)
    self.assertEqual(mock_request.mock_calls, [request_mock_call,
                                               request_mock_call,
                                               request_mock_call,
                                               request_mock_call])
    self.assertEqual(mock_time.sleep.mock_calls,
                     [mock.call(SLEEP_TIME_SECONDS),
                      mock.call(SLEEP_TIME_SECONDS),
                      mock.call(SLEEP_TIME_SECONDS)])
    self.assertEqual(result, self.mock_response)

  @mock.patch('metadata_watcher.time')
  @mock.patch('urllib2.urlopen')
  @mock.patch('urllib2.Request')
  def testSendRequestShouldRaiseErrorWhenSupportedExceptionCaughtButTimeout(
      self, mock_request, mock_urlopen, mock_time):
    mock_time.time.side_effect = [20]
    mock_request.return_value = mock_request
    mock_urlopen.side_effect = self._HttpErrorWithCode(
        httplib.SERVICE_UNAVAILABLE)
    subject = metadata_watcher.MetadataWatcher()
    subject.start_time = 0

    with self.assertRaises(urllib2.HTTPError):
      subject._SendRequest('key', timeout_seconds=10)

  @mock.patch('urllib2.urlopen')
  @mock.patch('urllib2.Request')
  def testSendRequestShouldRaiseErrorWhenNonSupportedExceptionCaught(
      self, mock_request, mock_urlopen):
    mock_request.return_value = mock_request
    mock_urlopen.side_effect = self._HttpErrorWithCode(httplib.BAD_REQUEST)

    with self.assertRaises(urllib2.HTTPError):
      metadata_watcher.MetadataWatcher()._SendRequest('key')

  @mock.patch('urllib2.urlopen')
  @mock.patch('urllib2.Request')
  def testGetAttributeUpdateShouldReturnValueInResponse(
      self, mock_request, mock_urlopen):
    mock_request.return_value = mock_request
    mock_urlopen.return_value = self.mock_response
    self.mock_response.read.return_value = 'value'

    result = metadata_watcher.MetadataWatcher()._GetAttributeUpdate('key')

    self.mock_response.headers.get.assert_called_once_with('etag', None)
    self.assertEqual(result, 'value')

  @mock.patch('urllib2.urlopen')
  @mock.patch('urllib2.Request')
  def testGetAttributeUpdateShouldUpdateEtag(
      self, mock_request, mock_urlopen):
    mock_request.return_value = mock_request
    mock_urlopen.return_value = self.mock_response
    self.mock_response.headers.return_value = ETAG
    subject = metadata_watcher.MetadataWatcher()

    self.assertNotEqual(subject.etag, ETAG)

    subject._GetAttributeUpdate('key')

    self.assertEqual(subject.etag, ETAG)

  @mock.patch('urllib2.urlopen')
  @mock.patch('urllib2.Request')
  def testGetAttributeUpdateShouldRetryWhenEtagEqualsAndNotTimeout(
      self, mock_request, mock_urlopen):
    mock_request.return_value = mock_request
    mock_urlopen.return_value = self.mock_response
    self.mock_response.headers.get.side_effect = [ETAG, NEW_ETAG]
    self.mock_response.read.return_value = 'value'
    subject = metadata_watcher.MetadataWatcher(etag=ETAG)

    result = subject._GetAttributeUpdate('key')

    self.assertEqual(self.mock_response.headers.get.mock_calls,
                     [mock.call('etag', ETAG), mock.call('etag', ETAG)])
    self.assertEqual(result, 'value')

  @mock.patch('urllib2.urlopen')
  @mock.patch('urllib2.Request')
  def testGetAttributeUpdateShouldRetryWhenValueIsEmptyAndNotTimeout(
      self, mock_request, mock_urlopen):
    mock_request.return_value = mock_request
    mock_urlopen.return_value = self.mock_response
    self.mock_response.headers.get.side_effect = ['12', '13']
    self.mock_response.read.side_effect = ['', 'value']

    result = metadata_watcher.MetadataWatcher()._GetAttributeUpdate('key')

    self.assertEqual(self.mock_response.headers.get.mock_calls,
                     [mock.call('etag', None), mock.call('etag', ETAG)])
    self.assertEqual(result, 'value')

  @mock.patch('metadata_watcher.time')
  @mock.patch('urllib2.urlopen')
  @mock.patch('urllib2.Request')
  def testGetAttributeUpdateShouldNotRetryWhenTimeout(
      self, mock_request, mock_urlopen, mock_time):
    mock_time.time.side_effect = [20]
    mock_request.return_value = mock_request
    mock_urlopen.return_value = self.mock_response
    self.mock_response.headers.get.return_value = ETAG
    subject = metadata_watcher.MetadataWatcher(etag=ETAG)
    subject.start_time = 0

    subject._GetAttributeUpdate('key', timeout_seconds=10)

    self.mock_response.headers.get.assert_called_once_with('etag', ETAG)

  @mock.patch('urllib2.urlopen')
  @mock.patch('urllib2.Request')
  def testGetAttributeShouldReturnNonEmptyValue(
      self, mock_request, mock_urlopen):
    mock_request.return_value = mock_request
    mock_urlopen.return_value = self.mock_response
    self.mock_response.read.return_value = 'value'

    result = metadata_watcher.MetadataWatcher().GetAttribute('key')

    self.assertEqual(result, 'value')

  @mock.patch('urllib2.urlopen')
  @mock.patch('urllib2.Request')
  def testGetAttributeShouldExitWhenExceptionCaught(
      self, mock_request, mock_urlopen):
    mock_request.return_value = mock_request
    mock_urlopen.side_effect = self._HttpErrorWithCode(httplib.BAD_REQUEST)

    with self.assertRaises(SystemExit):
      metadata_watcher.MetadataWatcher().GetAttribute('key')

  @mock.patch('metadata_watcher.time')
  @mock.patch('urllib2.urlopen')
  @mock.patch('urllib2.Request')
  def testGetAttributeShouldExistWhenValueIsEmpty(
      self, mock_request, mock_urlopen, mock_time):
    mock_time.time.side_effect = [0, 20]
    mock_request.return_value = mock_request
    mock_urlopen.return_value = self.mock_response
    self.mock_response.read.return_value = ''

    with self.assertRaises(SystemExit):
      metadata_watcher.MetadataWatcher().GetAttribute('key', timeout_seconds=10)

  def _ExpectedUrl(self, key, timeout=TIMEOUT_SECONDS, etag=None):
    expected_params = {
        'last_etag': etag,
        'timeout_sec': timeout,
        'wait_for_change': True,
    }
    return '%s%s?%s' % (self.base_url, key, urllib.urlencode(expected_params))

  def _HttpErrorWithCode(self, code):
    return urllib2.HTTPError('fake_url', code, '', {}, None)


if __name__ == '__main__':
  unittest.main()
