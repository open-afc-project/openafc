
import os
import shutil
import tempfile
import unittest
from ..take_packages import main


class TestTakePackages(unittest.TestCase):

    def setUp(self):
        unittest.TestCase.setUp(self)
        self._testdir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self._testdir)
        self._testdir = None
        unittest.TestCase.tearDown(self)

    def test_split(self):
        inpath_a = os.path.join(self._testdir, 'input-a')
        os.mkdir(inpath_a)
        inpath_b = os.path.join(self._testdir, 'input-b')
        os.mkdir(inpath_b)
        outpath = os.path.join(self._testdir, 'output')

        self.assertFalse(os.path.exists(outpath))
        main(['take_packages', '--log=debug', inpath_a, inpath_b, outpath])
        self.assertTrue(os.path.exists(outpath))
