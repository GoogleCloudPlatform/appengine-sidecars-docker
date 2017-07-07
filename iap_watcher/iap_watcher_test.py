import os
import shutil
import tempfile
import unittest
import iap_watcher
from jsobject import Object


class TestIapVerifier(unittest.TestCase):

  class TestMetadataWatcher(Object):
    """Used to mock out metadata watcher."""

    def WatchMetadata(self, function, **args):
      function(self.result_value_)

    def SetWatchMetadataResult(self, result_value):
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
    self.metadata_watcher_.SetWatchMetadataResult('{"enabled": true}')
    iap_watcher.Main(Object({
        'iap_metadata_key': 'AEF_IAP_state',
        'timeout': None,
        'output_state_file': path,
      }),
      watcher=self.metadata_watcher_,
      loop_watcher=True)
    self.assertTrue(os.path.isfile(path))

  def testDisabledNoEnable(self):
    """Tests for the state file does not exist on metadata disabled."""
    path = '%s/tmp' % self.pathname_
    self.metadata_watcher_.SetWatchMetadataResult('{"enabled": false}')
    iap_watcher.Main(Object({
        'iap_metadata_key': 'AEF_IAP_state',
        'timeout': None,
        'output_state_file': path,
      }),
      watcher=self.metadata_watcher_,
      loop_watcher=True)
    self.assertFalse(os.path.isfile(path))

  def testEnableDisable(self):
    """Tests for the state file upon switched states."""
    path = '%s/tmp' % self.pathname_
    self.metadata_watcher_.SetWatchMetadataResult('{"enabled": true}')
    iap_watcher.Main(Object({
        'iap_metadata_key': 'AEF_IAP_state',
        'timeout': None,
        'output_state_file': path,
      }),
      watcher=self.metadata_watcher_,
      loop_watcher=True)
    self.assertTrue(os.path.isfile(path))

    self.metadata_watcher_.SetWatchMetadataResult('{"enabled": false}')
    iap_watcher.Main(Object({
        'iap_metadata_key': 'AEF_IAP_state',
        'timeout': None,
        'output_state_file': path,
      }),
      watcher=self.metadata_watcher_,
      loop_watcher=True)
    self.assertFalse(os.path.isfile(path))

if __name__ == '__main__':
  unittest.main()
