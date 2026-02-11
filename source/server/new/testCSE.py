#
#	testCSE.py
#
#	(c) 2020 by Andreas Kraft
#	License: BSD 3-Clause License. See the LICENSE file for further details.
#
#	Unit tests for CSE functionality
#

import unittest, sys
import isodate
if '..' not in sys.path:
	sys.path.append('..')
from acme.etc.Types import ResponseStatusCode as RC
from acme.etc.Types import ResourceTypes as T
from init import *


class TestCSE(unittest.TestCase):

	TINYIOT_SUPPORTED_TYPES = [1, 2, 3, 4, 5, 9, 16, 23, 10001, 10002, 10003, 10004, 10005]

	@classmethod
	@unittest.skipIf(noCSE, 'No CSEBase')
	def setUpClass(cls) -> None:
		testCaseStart('Setup TestCSE')
		...
		testCaseEnd('Setup TestCSE')


	@classmethod
	@unittest.skipIf(noCSE, 'No CSEBase')
	def tearDownClass(cls) -> None:
		if not isTearDownEnabled():
			return
		testCaseStart('TearDown TestCSE')
		...
		testCaseEnd('TearDown TestCSE')


	def setUp(self) -> None:
		testCaseStart(self._testMethodName)
	

	def tearDown(self) -> None:
		testCaseEnd(self._testMethodName)


	#########################################################################


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveCSE(self) -> None:
		"""	Retrieve <CB> """
		_, rsc = RETRIEVE(cseURL, ORIGINATOR)
		self.assertEqual(rsc, RC.OK)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveCSEWithWrongOriginator(self) -> None:
		""" Retrieve <CB> with wrong originator -> Fail """
		_, rsc = RETRIEVE(cseURL, 'CWrong')
		self.assertEqual(rsc, RC.ORIGINATOR_HAS_NO_PRIVILEGE)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_attributesCSE(self) -> None:
		"""	Validate <CB> attributes """
		r, rsc = RETRIEVE(cseURL, ORIGINATOR)
		self.assertEqual(rsc, RC.OK)
		self.assertEqual(findXPath(r, 'm2m:cb/csi')[0], '/')
		self.assertEqual(findXPath(r, 'm2m:cb/csi'), CSEID)
		self.assertEqual(findXPath(r, 'm2m:cb/pi'), '')
		self.assertEqual(findXPath(r, 'm2m:cb/rr'), False)
		print('rr value:', findXPath(r, 'm2m:cb/rr'), type(findXPath(r, 'm2m:cb/rr')))
		self.assertEqual(findXPath(r, 'm2m:cb/rn'), CSERN)
		self.assertEqual(findXPath(r, 'm2m:cb/ty'), 5)
		self.assertEqual(findXPath(r, 'm2m:cb/ri'), CSEID[1:])
		self.assertIsNotNone(findXPath(r, 'm2m:cb/ct'))
		self.assertIsNotNone(findXPath(r, 'm2m:cb/cst'))

		self.assertIsNotNone(srt := findXPath(r, 'm2m:cb/srt'))
		self.assertIsInstance(srt, list)
		self.assertEqual(sorted(srt), sorted(TestCSE.TINYIOT_SUPPORTED_TYPES))
		self.assertIsNotNone(srv := findXPath(r, 'm2m:cb/srv'))
		self.assertIsInstance(srv, list)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_CSEreleaseVersion(self) -> None:
		"""	Test the release version """
		r, rsc = RETRIEVE(cseURL, ORIGINATOR)
		self.assertEqual(rsc, RC.OK)
		parameters = lastHeaders()
		self.assertIsNotNone(parameters)
		self.assertIn('X-M2M-RVI', parameters)
		self.assertIsNotNone(rvi := parameters.get('X-M2M-RVI'))
		self.assertIsNotNone(srv := findXPath(r, 'm2m:cb/srv'))
		self.assertIn(rvi, srv)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_CSESupportedResourceTypes(self) -> None:
		"""	Check <CB> SRT """
		r, rsc = RETRIEVE(cseURL, ORIGINATOR)
		self.assertEqual(rsc, RC.OK)
		self.assertIsNotNone(srt := findXPath(r, 'm2m:cb/srt'))
		print('srt value:', srt, type(srt))
		self.assertEqual(sorted(srt), sorted(TestCSE.TINYIOT_SUPPORTED_TYPES))
		
		#for t in T.supportedResourceTypes():	#  type: ignore
		#	self.assertIn(t, srt)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_deleteCSEFail(self) -> None:
		"""	Delete <CB> -> Fail """
		_, rsc = DELETE(cseURL, ORIGINATOR)
		self.assertEqual(rsc, RC.OPERATION_NOT_ALLOWED)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_updateCSEFail(self) -> None:
		"""	Update <CB> -> Fail """
		dct = 	{ 'm2m:cse' : {
					'lbl' : [ 'aTag' ]
				}}
		_, rsc = UPDATE(cseURL, ORIGINATOR, dct)
		self.assertEqual(rsc, RC.OPERATION_NOT_ALLOWED)


def run(testFailFast:bool) -> TestResult:

	# Assign tests
	suite = unittest.TestSuite()
	addTests(suite, TestCSE, [

		'test_retrieveCSE',
		'test_retrieveCSEWithWrongOriginator',
		'test_attributesCSE',
		'test_CSESupportedResourceTypes',
		'test_deleteCSEFail',
		'test_updateCSEFail',
		'test_CSEreleaseVersion',
	])

	# Run tests
	result = unittest.TextTestRunner(verbosity=testVerbosity, failfast=testFailFast).run(suite)
	printResult(result)
	return result.testsRun, len(result.errors + result.failures), len(result.skipped), getSleepTimeCount()

if __name__ == '__main__':
	r, errors, s, t = run(False)
	sys.exit(errors)
