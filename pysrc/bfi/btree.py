import bsddb, struct
from plain import PlainBFI

class BTreeBFI(object):

    def __init__(self, filename):
        self.bfi = PlainBFI(filename)
        self.index = bsddb.btopen('%s.pk' % filename, 'c')

    def append(self, pk, items):
        slot = struct.pack("I", self.bfi.append(items))
        self.index["S" + slot] = pk
        self.index["R" + pk] = slot

    def write(self, pk, items):
        slot, = struct.unpack("I", self.index["R" + pk])
        self.bfi.write(slot, items)

    def lookup(self, items):
        slots = self.bfi.lookup(items)
        return [ self.index["S" + struct.pack("I", x)] for x in slots ]

    def sync(self):
        self.bfi.sync()
        self.index.sync()

    def close(self):
        self.bfi.close()
        self.index.close()
