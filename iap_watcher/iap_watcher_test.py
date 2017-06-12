import tempfile
import unittest
import iap_verifier

class TestIapVerifier(unittest.TestCase):

	def setUpModule(self):
		self.handler_, self.pathname_ = tempfile.mkstemp()

	def tearDownClass(self):
		self.handler_.close()

	def testKeyFile(self):
		iap_verifier.Main({
			'key': 'AEF_IAP_state',
			'output_state_file': self.pathname_,
		})

if __name__ == '__main__':
	unittest.main()