"""
OO Wrapper for bfi functions

>>> index = BloomFilterIndex('test.bfi')
>>> [ index.add(x, ['FIRST-%d' % x, 'SECOND-%d' % (x % 3)]) for x in range(10) ]
[0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
>>> index.sync()
10
>>> index.lookup(['FIRST-4'])
[4]
>>> index.lookup(['SECOND-2'])
[2, 5, 8]
>>> import os
>>> os.unlink('test.bfi')

"""
import _bfi

class BloomFilterIndex(object):
    
    def __init__(self, filename):
        self.filename = filename
        self._ptr = _bfi.bfi_open(filename)
        
    def __del__(self):
        ' Try and clean up '
        if hasattr(self, '_ptr') and self._ptr is not None:
            self.close()
        
    def close(self):
        if self._ptr is None: raise RuntimeError("Index is closed")
        _bfi.bfi_close(self._ptr);
        self._ptr = None

    def sync(self):
        if self._ptr is None: raise RuntimeError("Index is closed")
        return _bfi.bfi_sync(self._ptr)
	
    def append(self, pk, values):
        if self._ptr is None: raise RuntimeError("Index is closed")
        return _bfi.bfi_append(self._ptr, pk, values)
        
    def insert(self, pk, values):
        if self._ptr is None: raise RuntimeError("Index is closed")
        return _bfi.bfi_insert(self._ptr, pk, values)
        
    def delete(self, pk):
        if self._ptr is None: raise RuntimeError("Index is closed")
        return _bfi.bfi_delete(self._ptr, pk)
        
    def lookup(self, values):
        if self._ptr is None: raise RuntimeError("Index is closed")
        #print "PYTHON INDEX", self._index
        return _bfi.bfi_lookup(self._ptr, values)
	
    def stat(self):
        return _bfi.bfi_stat(self._ptr)
        
    def __repr__(self):
        return "<BloomFilterIndex %s>" % self.filename
        
if __name__ == '__main__':
    import doctest
    print doctest.testmod()
