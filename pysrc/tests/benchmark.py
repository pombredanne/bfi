import bfi, sqlite3, bsddb
import os, struct

FIELDS = 10
RS = '\x1E'

def wrapper(func, *args, **kwargs):
    def wrapped():
        return func(*args, **kwargs)
    return wrapped

def report(method, time, count=1):
    print "%30s:    %0.5fs" % (method, time/count)

def create_index(obj, count):
    
    for i in range(count):
        obj.add(i, ['FOO_%d:This is a test %d' % (x, i) for x in range(FIELDS)])
    
    obj.sync()

class SQLiteProxy(object):
    
    def __init__(self, filename):
        self.conn = sqlite3.connect(filename)
        
        self.conn.isolation_level = None
        self.in_transaction = False
        
        self.cursor = self.conn.cursor()
        
        query = 'CREATE TABLE data (pk INTEGER PRIMARY KEY, %s)' % ', '.join(['FOO_%d' % x for x in range(FIELDS)])
        self.cursor.execute(query)
        
        self.insert = 'INSERT INTO data VALUES (?, %s)' % ', '.join(['?'] * FIELDS)
        
    def add(self, pk, items):
        
        if not self.in_transaction:
            self.cursor.execute("BEGIN")
            self.in_transaction = True
        
        values = [ x.split(':')[1] for x in items ]
        
        self.cursor.execute(self.insert, [pk] + values)
        
    def sync(self):
        if self.in_transaction:
            self.cursor.execute("COMMIT")
            self.in_transaction = False
            
    def close(self):
        self.cursor = None
        self.conn.close()
        
    def lookup(self, filters):
        params = [ x.split(':') for x in filters ]
        #print params
        
        query = "SELECT pk FROM data WHERE %s" % ' AND '.join(['%s=?' % x[0] for x in params])
        #print query
        
        self.cursor.execute(query, [ x[1] for x in params ])
        
        return [ x[0] for x in self.cursor.fetchall() ]

class BTreeProxy(object):
    
    def __init__(self, filename):
        os.mkdir(filename)

        self.dbs = {}
        for i in range(FIELDS):
            self.dbs['FOO_%d' % i] = bsddb.btopen('%s/FOO_%d.db' % (filename, i))
            
        self.directory = filename

    def close(self):
        for i in range(FIELDS):
            self.dbs['FOO_%d' % i].close();
            #os.unlink('%s/FOO_%d.db' % (self.directory, i))
        #os.rmdir(self.directory)
            
    def sync(self): 
        for db in self.dbs.values():
            db.sync()
            
    def add(self, pk, items):
        
        for x in items:
            parts = x.split(':')
            key = "%s%s%s" % (parts[1], RS, struct.pack('I', pk))
            self.dbs[parts[0]][key] = None
            
    def lookup(self, items):
        results = []
    
        for filter in items:
            key, value = filter.split(':')
            value += RS
    
            db = self.dbs[key]
    
            matches = set()
            k, _ = db.set_location(value)
            while k.startswith(value):
                pk, = struct.unpack('I', k[-4:])
                matches.add(pk)
                k, _ = db.next()
            # print "MATCHES", matches
    
            results.append(matches)
    
        
        result = results[0]
        for i in range(1, len(items)):
            result &= results[i]
    
        result = list(result)
        result.sort()
        return result
        

def run_benchmarks(cls, filename, fields=10, count=100000):
    import timeit
    
    if os.path.exists(filename): os.unlink(filename)
    
    obj = cls(filename)
    
    result = timeit.timeit(wrapper(create_index, obj, count), number=1)
    report("INDEX", result, 1)
    
    #print obj.lookup(['FOO_6:This is a test 82435'])
    assert obj.lookup(['FOO_6:This is a test 82435']) == [82435]
    result = timeit.timeit(wrapper(obj.lookup, ['FOO_6:This is a test 82435']), number=100)
    report("LOOKUP (1)", result, 100)
    assert obj.lookup(['FOO_6:This is a test 82435', 
                     'FOO_8:This is a test 82435', 'FOO_2:This is a test 82435']) == [82435]
    result = timeit.timeit(wrapper(obj.lookup, ['FOO_6:This is a test 82435', 
            'FOO_8:This is a test 82435', 'FOO_2:This is a test 82435']), number=100)
    report("LOOKUP (3)", result, 100)
    
    print "%30s: %dMB" % ("SIZE", os.stat(filename).st_size / 1000000)

if __name__ == '__main__':
    
    print "## BFI ##"
    run_benchmarks(bfi.BloomFilterIndex, 'test.bfi')
    print "### SQLite3 ###"
    run_benchmarks(SQLiteProxy, 'test.sq3')
    print '### BTree ###'
    run_benchmarks(BTreeProxy, 'test.db')