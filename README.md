# Bloom Filter Index

**Pre-alpha code - don't even bother checking out yet**

## Overview

An experimental new index designed for document-based databases using bloom filters.  Allows for fast full table scans without needing to deserialize the
documents.

See [implementation](implementation.md) details.

## Features

* Indexing of **ALL** record fields, including sub-elements.
* Near linear time regardless of number of fields searched for.
* Simple add, remove, update - no tree to rebalance.
* Compact structure compared to multiple btrees.
* Low CPU usage - no need to decompress and deserialize data.

## Performance

Early days but showing promise.  

Dataset: 100,000 records with a 32bit integer primary key.  Each record consists of 10 fields with a length of 16-20 bytes. Tests were run on an i7 laptop with solid state drive.

```
## BFI ##
                         INDEX:    1.01441s
                    LOOKUP (1):    0.00317s
                    LOOKUP (3):    0.00555s
                          SIZE: 13MB
### SQLite3 ###
                         INDEX:    1.70589s
                    LOOKUP (1):    0.04929s
                    LOOKUP (3):    0.04759s
                          SIZE: 25MB
### BTree ###
                         INDEX:    5.77204s
                    LOOKUP (1):    0.00001s
                    LOOKUP (3):    0.00002s
                          SIZE: 66MB
```

Obviously nothing can compare to BTree for speed O(log n) compared to O(n), but I have included it for reference, particularly to demonstrate the storage overhead involved.  3ms full table scan for BFI should be adequate for all but the highest performance applications - after all it scales linearly which equates to 31.5 million records per second (and it should be possible to increase that with mmap and pagesize tweeks)

BFI is around 10 times faster than SQLite for the full table scans - need to test against a higher performance implementation such as MySQL as well as some NoSQL engines like MongoDB.

Another factor that makes a significant storage impact on NoSQL datasets is that primary keys are typically larger than 4 or 8 bytes as used by RDBs - Mongo has managed to squeeze it to 12bytes, other DBs use 20byte UUIDs.  When using BTree the primary key has to be stored with every value which makes indexes expensive - and often larger than the values indexed.  BFI stores each primary key only once.  If we use 12 byte primary keys in the above examples it adds another 8MB, while the BFI datastructure would only grow by 0.8MB.

One last point to make is that if we increased this to 30 fields, only the indexing time of BFI would go up (x3), not the storage requirements or query times.  This is ideal for document-orientated databases where developers should be storing all immediately related data in the document rather than normalising.

## Limitations

* No range queries - though user defined ranges can be used e.g ``AgeRange:18-30``
* Probabilistic nature of bloom filters requires validation of results.
* Current implementataion designed to keep probability below 0.001 for up to 30 fields indexed.  Need to add options for large field sets.

## TODO

* Turn into a usable library
* Implement mmap for lookup - should give significant boost
* Get someone who can actually write C to go over the code.
* Add comparison to current NoSQL full table scan speeds.
* Add comparison to proper(!) RDB (MySQL)
