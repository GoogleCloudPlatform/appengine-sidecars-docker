import httplib
import os
import shutil
import tempfile
import unittest
import iap_watcher
from jsobject import Object


class TestIapVerifier(unittest.TestCase):

  class TestMetadataWatcher(Object):
    """Used to mock out metadata watcher."""

    def GetMetadata(self, **args):
      return self.result_value_

    def SetGetMetadataResult(self, result_value):
      self.result_value_ = result_value

  @classmethod
  def setUpClass(cls):
    cls.metadata_watcher_ = cls.TestMetadataWatcher()
    cls.pathname_ = tempfile.mkdtemp()

  @classmethod
  def tearDownClass(cls):
    shutil.rmtree(cls.pathname_)

  def testEnable(self):
    """Tests for the state file exists on metadata enable."""
    path = '%s/tmp' % self.pathname_
    self.metadata_watcher_.SetGetMetadataResult('{"enabled": true}')
    iap_watcher.Main(Object({
        'iap_metadata_key': 'AEF_IAP_state',
        'output_state_file': path,
        'polling_interval': 1,
        'fetch_keys': False,
      }),
      watcher=self.metadata_watcher_,
      loop_watcher=False)
    self.assertTrue(os.path.isfile(path))

  def testDisable(self):
    """Tests for the state file does not exist on metadata disabled."""
    path = '%s/tmp' % self.pathname_
    self.metadata_watcher_.SetGetMetadataResult('{"enabled": false}')
    iap_watcher.Main(Object({
        'iap_metadata_key': 'AEF_IAP_state',
        'output_state_file': path,
        'polling_interval': 1,
        'fetch_keys': False,
      }),
      watcher=self.metadata_watcher_,
      loop_watcher=False)
    self.assertFalse(os.path.isfile(path))

  def testEnableDisable(self):
    """Tests for the state file upon switching to enabled."""
    path = '%s/tmp' % self.pathname_
    self.metadata_watcher_.SetGetMetadataResult('{"enabled": true}')
    iap_watcher.Main(Object({
        'iap_metadata_key': 'AEF_IAP_state',
        'output_state_file': path,
        'polling_interval': 1,
        'fetch_keys': False,
      }),
      watcher=self.metadata_watcher_,
      loop_watcher=False)
    self.assertTrue(os.path.isfile(path))

    self.metadata_watcher_.SetGetMetadataResult('{"enabled": false}')
    iap_watcher.Main(Object({
        'iap_metadata_key': 'AEF_IAP_state',
        'output_state_file': path,
        'polling_interval': 1,
        'fetch_keys': False,
      }),
      watcher=self.metadata_watcher_,
      loop_watcher=False)
    self.assertFalse(os.path.isfile(path))

  def testDisableEnable(self):
    """Tests for the state file upon switching to disabled."""
    path = '%s/tmp' % self.pathname_
    self.metadata_watcher_.SetGetMetadataResult('{"enabled": true}')
    iap_watcher.Main(Object({
        'iap_metadata_key': 'AEF_IAP_state',
        'output_state_file': path,
        'polling_interval': 1,
        'fetch_keys': False,
      }),
      watcher=self.metadata_watcher_,
      loop_watcher=False)
    self.assertTrue(os.path.isfile(path))

    self.metadata_watcher_.SetGetMetadataResult('{"enabled": false}')
    iap_watcher.Main(Object({
        'iap_metadata_key': 'AEF_IAP_state',
        'output_state_file': path,
        'polling_interval': 1,
        'fetch_keys': False,
      }),
      watcher=self.metadata_watcher_,
      loop_watcher=False)
    self.assertFalse(os.path.isfile(path))

    self.metadata_watcher_.SetGetMetadataResult('{"enabled": true}')
    iap_watcher.Main(Object({
        'iap_metadata_key': 'AEF_IAP_state',
        'output_state_file': path,
        'polling_interval': 1,
        'fetch_keys': False,
      }),
      watcher=self.metadata_watcher_,
      loop_watcher=False)
    self.assertTrue(os.path.isfile(path))

  def testFetchKeys(self):
    """Tests that keys are fetched when the fetch_keys argument is True."""
    state_path = '%s/tmp' % self.pathname_
    key_path = '%s/keys' % self.pathname_
    self.metadata_watcher_.SetGetMetadataResult('{"enabled": true}')
    def mock_os_system(command):
      self.cmd = command
    iap_watcher.Main(Object({
        'iap_metadata_key': 'AEF_IAP_state',
        'output_state_file': state_path,
        'polling_interval': 1,
        'fetch_keys': True,
        'output_key_file': key_path,
      }),
      watcher=self.metadata_watcher_,
      loop_watcher=False,
      os_system=mock_os_system)
    self.assertEqual(
        self.cmd,
        'curl "https://www.gstatic.com/iap/verify/public_key-jwk" > '
            + key_path)


if __name__ == '__main__':
  unittest.main()
