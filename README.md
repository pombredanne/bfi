# Bloom Filter Index

**Pre-alpha code - don't even bother checking out yet**

## Overview

An experimental new index designed for document-based databases using bloom filters.  Allows for fast full table scans without needing to deserialize the
documents.

## Features

* Indexing of **ALL** record fields, including sub-elements.
* Linear time regardless of number of fields searched for.
* Simple add, remove, update - no tree to rebalance.
* Compact structure compared to multiple btrees.
* Low CPU usage - no need to decompress and deserialize data.

## Performance

Early days but showing promise.  This really needs comparing to real world full table scans on both RMDB and Document based databases, but I have included BTree
for the moment as this is predominant method of adding non-key indexes to document orientated databases.

On a dataset of 470,000 items with 10 fields indexed and a 32bit integer primary key. 

|                       | BFI       | BSDDB btree           |
------------------------|-----------|------------------------
| Index time            | 11.4s     | 40.8s                 |
| Size:                 | 60MB      | 12-18MB * 10 ~= 140MB |
| Search time           | O(n)      | O(logn)               |
| Query time (1 item)   | 0.018s    | 0.004s                |
| Query time (3 items)  | 0.033s    | 0.275s                |

The BSDDB timings are run via a python timeit so should be pretty accurate but are not like-for-like.

The characteristics of BFI are favourable both in terms of storage and index size.  BTree naturally wins hands down on a single query as it should only require around 20 seeks with a well balanced tree, though sub 20ms is pretty respectable for BFI.  Where multiple fields are queried simultaneously BFI has an advantange (though the BTree implementation is rather quick and dirty).

The other factor that makes a significant storage impact on NoSQL datasets is that primary keys are typically larger than RMDB - Mongo has managed to squeeze it to 12bytes, other DBs use 20byte UUIDs.  When using BTree the primary key has to be stored with every value which makes indexes expensive - and often larger than the data indexed.  BFI stores each primary key only once.  If we use 12 byte primary keys in the above examples it adds 3.8MB to each btree, 38MB in total, while the BFI datastructure would only grow by 3.8MB.

## Limitations

* No range queries - though user defined ranges can be used e.g ``AgeRange:18-30``
* Probabilistic nature of bloom filters requires validation of results.
* Current implementataion designed to keep probability below 0.001 for up to 30 fields indexed.  Need to 

## TODO

* Implement mmap for lookup - should give significant boost
* Get someone who can actually write C to go over the code.