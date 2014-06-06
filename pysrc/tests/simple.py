from bfi import BloomFilterIndex
import os

INDEX_FILE = 'test.bfi'

if os.path.exists(INDEX_FILE): os.unlink(INDEX_FILE)

index = BloomFilterIndex(INDEX_FILE);

print "REPR", repr(index)

[ index.add(x, ['FIRST-%d' % x, 'SECOND-%d' % (x % 3)]) for x in range(10) ]

print "SYNC", index.sync()

print "FIRST", index.lookup(['FIRST-4'])
print "SECOND", index.lookup(['SECOND-2'])

print "COMBINED", index.lookup(['FIRST-5', 'SECOND-2'])

print "STAT", index.stat()

index.close()

os.unlink(INDEX_FILE);
