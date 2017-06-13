import os
import tempfile
import unittest
import iap_watcher
from jsobject import Object

class TestIapVerifier(unittest.TestCase):

	class TestMetadataWatcher:
		"""Used to mock out metadata watcher."""
		def WatchMetadata(self, function, **args):
			function(self.result_value_)

		def SetWatchMetadataResult(self, result_value):
			self.result_value_ = result_value

	@classmethod
	def setUpClass(cls):
		cls.metadata_watcher_ = cls.TestMetadataWatcher()
		handler, cls.pathname_ = tempfile.mkstemp()
		cls.handler_ = os.fdopen(handler)

	@classmethod
	def tearDownClass(cls):
		cls.handler_.close()
		os.remove(cls.pathname_)

	def testKeyFile(self):
		self.metadata_watcher_.SetWatchMetadataResult("test123")
		iap_watcher.Main(Object({
			'key': 'AEF_IAP_state',
			'timeout': None,
			'output_state_file': self.pathname_,
		}),
		watcher=self.metadata_watcher_,
		post_update=None)
		self.assertEqual("test123", self.handler_.read())

if __name__ == '__main__':
	unittest.main()