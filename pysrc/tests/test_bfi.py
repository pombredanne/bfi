import unittest, os
import bfi

class BFITestCase(unittest.TestCase):

    TEST_FILE = 'test.bfi'

    def tearDown(self):
        if os.path.exists(self.TEST_FILE):
            os.unlink(self.TEST_FILE)

    def load_data(self, count=1500):
        index = bfi.BFI(self.TEST_FILE)

        [ index.append(['FIRST-%d' % x, 'SECOND-%d' % (x % 73)]) for x in range(count) ]

        self.assertEqual(index.sync(), count)

        return index

    def test_load(self):
        index = self.load_data()

        info = index.stat()
        self.assertEqual(info['slots'], 1500)
        self.assertEqual(info['version'], 3)
        self.assertEqual(info['pages'], 3)

    def test_lookup(self):
        index = self.load_data()

        self.assertEqual(index.lookup(['FIRST-576']), [576])

        expected = [ 45 + (x * 73) for x in range(20) ]
        self.assertEqual(index.lookup(['SECOND-45']), expected)


        self.assertEqual(index.lookup(['FIRST-576', 'SECOND-1']), [])
        self.assertEqual(index.lookup(['FIRST-576', 'SECOND-%d' % (576 % 73)]), [576])

    def test_update(self):
        index = self.load_data()

        index.write(576, ['FOO', 'BAR'])

        self.assertEqual(index.lookup(['BAR']), [576])
        self.assertEqual(index.lookup(['FIRST-577']), [577])

    def test_bad_input(self):
        index = bfi.BFI(self.TEST_FILE)

        with self.assertRaisesRegexp(ValueError, 'Need at least one value to index'):
            index.append('Test')

        with self.assertRaisesRegexp(ValueError, 'Need at least one value to lookup'):
            index.lookup('Test')

    def test_bad_file(self):

        with self.assertRaisesRegexp(IOError, 'Permission denied'):
            index = bfi.BFI('/root/foo.db')

        with self.assertRaisesRegexp(IOError, 'Not a bloom index'):
            index = bfi.BFI(__file__)
