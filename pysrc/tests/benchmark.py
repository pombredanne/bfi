import sqlite3, bsddb
from bfi.btree import BTreeBFI
import os, struct

try:
    import pymongo
    HAS_MONGO = True
except ImportError:
    HAS_MONGO = False

FIELDS = 10
RECORDS = 100000
RS = '\x1E'

def wrapper(func, *args, **kwargs):
    def wrapped():
        return func(*args, **kwargs)
    return wrapped

def report(method, time, count=1):
    print "%30s:    %0.5fs" % (method, time/count)

def create_index(obj, count):
    
    for i in range(count):
        obj.append("PK-%d" % i, ['FOO_%d:This is a test %d' % (x, i) for x in range(FIELDS)])
    
    obj.sync()

class SQLiteProxy(object):
    
    def __init__(self, filename):
        self.conn = sqlite3.connect(filename)
        
        self.conn.isolation_level = None
        self.in_transaction = False
        
        self.cursor = self.conn.cursor()
        
        query = 'CREATE TABLE data (pk text PRIMARY KEY, %s)' % ', '.join(['FOO_%d text' % x for x in range(FIELDS)])
        self.cursor.execute(query)
        
        self.insert = 'INSERT INTO data VALUES (?, %s)' % ', '.join(['?'] * FIELDS)
        
    def append(self, pk, items):
        
        if not self.in_transaction:
            self.cursor.execute("BEGIN")
            self.in_transaction = True
        
        values = [pk] + [ x.split(':')[1] for x in items ]
        
        self.cursor.execute(self.insert, values)
        
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
            os.unlink('%s/FOO_%d.db' % (self.directory, i))
        os.rmdir(self.directory)
            
    def sync(self): 
        for db in self.dbs.values():
            db.sync()
            
    def append(self, pk, items):
        
        for x in items:
            parts = x.split(':')
            key = "%s%s%s" % (parts[1], RS, pk)
            self.dbs[parts[0]][key] = None
            
    def lookup(self, items):
        results = []
    
        for filter in items:
            key, value = filter.split(':')
            value += RS
            c = len(value)
    
            db = self.dbs[key]
    
            matches = set()
            k, _ = db.set_location(value)
            while k.startswith(value):
                pk = k[c:]
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

    def size(self):
        return "Unknown"

class MongoProxy(object):

    DB = 'test_database'
    COLLECTION = 'benchmark_data'

    def __init__(self, filename):
        self.client = pymongo.MongoClient()
        self.db = self.client[self.DB]
        if self.COLLECTION in self.db.collection_names():
            self.db.drop_collection(self.COLLECTION)
        self.collection = self.db[self.COLLECTION]

    def add(self, pk, items):
        data = dict([ x.split(":") for x in items ])

        data['_id'] = pk
        self.collection.insert(data)

    def sync(self): pass

    def close(self):
        self.collection = None
        self.db = None
        self.client.close()

    def lookup(self, items):
	query = dict([ x.split(":") for x in items ])
	
        result = [ x['_id'] for x in self.collection.find(query)]
        return result

    def size(self):
        stats = self.db.stats()
	return "%dMB (+%dMB indexes)" % (stats['dataSize'] / 1000000, stats['indexSize'] / 1000000)

def run_benchmarks(cls, filename, fields=10, count=RECORDS):
    import timeit

    RECORD = 83376
    
    if filename and os.path.isfile(filename): os.unlink(filename)
    
    obj = cls(filename)
    
    result = timeit.timeit(wrapper(create_index, obj, count), number=1)
    report("INDEX", result, 1)
    
    #print obj.lookup(['FOO_6:This is a test %d' % RECORD])
    assert obj.lookup(['FOO_6:This is a test %d' % RECORD]) == ["PK-%d" % RECORD]
    result = timeit.timeit(wrapper(obj.lookup, ['FOO_6:This is a test %d' % RECORD]), number=100)
    report("LOOKUP (1)", result, 100)
    query = ['FOO_6:This is a test %d' % RECORD, 
                     'FOO_8:This is a test %d' % RECORD,
                     'FOO_2:This is a test %d' % RECORD]
    assert obj.lookup(query) == ["PK-%d" % RECORD]
    result = timeit.timeit(wrapper(obj.lookup, query), number=100)
    report("LOOKUP (3)", result, 100)
    
    if filename and os.path.isfile(filename):
        print "%30s: %dMB" % ("SIZE", os.stat(filename).st_size / 1000000)
    else:
        print "%30s: %s" % ("SIZE", obj.size())

    obj.close()

if __name__ == '__main__':
    
    print "## BFI ##"
    run_benchmarks(BTreeBFI, 'test.bfi')
    print "### SQLite3 ###"
    run_benchmarks(SQLiteProxy, 'test.sq3')
    print '### BTree ###'
    run_benchmarks(BTreeProxy, 'test.db')
    if HAS_MONGO:
        print "### MongoDB ###"
        run_benchmarks(MongoProxy, None)
