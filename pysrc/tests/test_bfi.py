import unittest, os
import bfi

class BFITestCase(unittest.TestCase):

    TEST_FILE = 'test.bfi'

    def tearDown(self):
        if os.path.exists(self.TEST_FILE):
            os.unlink(self.TEST_FILE)

    def load_data(self, count=1500):
        index = bfi.BloomFilterIndex(self.TEST_FILE)

        [ index.append(x, ['FIRST-%d' % x, 'SECOND-%d' % (x % 73)]) for x in range(count) ]

        self.assertEqual(index.sync(), count)

        return index

    def test_load(self):
        index = self.load_data()

        info = index.stat()
        self.assertEqual(info['records'], 1500)
        self.assertEqual(info['version'], 3)
        self.assertEqual(info['pages'], 2)

    def test_lookup(self):
        index = self.load_data()

        self.assertEqual(index.lookup(['FIRST-576']), [576])

        expected = [ 45 + (x * 73) for x in range(20) ]
        self.assertEqual(index.lookup(['SECOND-45']), expected)


        self.assertEqual(index.lookup(['FIRST-576', 'SECOND-1']), [])
        self.assertEqual(index.lookup(['FIRST-576', 'SECOND-%d' % (576 % 73)]), [576])

    def test_insert(self):
        index = self.load_data()

        index.insert(576, ['FOO', 'BAR'])

        self.assertEqual(index.lookup(['BAR']), [576])
        self.assertEqual(index.lookup(['FIRST-577']), [577])

    def test_delete(self):
        index = self.load_data()

        index.delete(576)
        self.assertEqual(index.stat()['records'], 1499)

        self.assertEqual(index.lookup(['FIRST-576']), [])
        self.assertEqual(index.lookup(['FIRST-577']), [577])
        
        index.insert(1501, ['FOO', 'BAR'])
        self.assertEqual(index.stat()['records'], 1500)

    def test_bad_input(self):
        index = bfi.BloomFilterIndex(self.TEST_FILE)

        with self.assertRaisesRegexp(ValueError, 'Need at least one value to index'):
            index.append(34, 'Test')

        with self.assertRaisesRegexp(ValueError, 'Need at least one value to lookup'):
            index.lookup('Test')

        with self.assertRaisesRegexp(TypeError, 'an integer is required'):
            index.append("test", ['foo'])

    def test_bad_file(self):

        with self.assertRaisesRegexp(IOError, 'Permission denied'):
            index = bfi.BloomFilterIndex('/root/foo.db')

        with self.assertRaisesRegexp(IOError, 'Not a bloom index'):
            index = bfi.BloomFilterIndex(__file__)
