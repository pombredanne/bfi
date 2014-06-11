# Bloom Filter Index

**Pre-alpha code**

## Overview

An experimental new index designed for document-based databases using [bloom filters](https://en.wikipedia.org/wiki/Bloom_filter).  Allows for fast full table scans without needing to deserialize the documents.

## Features

* Indexing of **ALL** record fields, including sub-elements.
* Near linear time regardless of number of fields searched for.
* Simple add, remove, update - no tree to rebalance.
* Compact structure compared to multiple btrees.
* Low CPU usage - no need to decompress and deserialize data.

## Installation

```bash
git clone git://github.com/tf198/bfi
cd bfi
make
make test
```

## Performance

Early days but showing promise.  

Dataset: 100,000 records with a 32bit integer primary key.  Each record consists of 10 fields with a length of 16-20 bytes. 

The two databases (SQLite and MongoDB) contain the record data and the times reflect full table scans without any indexes available.
BFI is a single file index containing all fields while the BTree example uses one file per field and does a simple set() union for the lookup(3) test.

Tests were run on a VM with 512MB memory.
```
## BFI ##
                         INDEX:    1.40005s
                    LOOKUP (1):    0.00213s
                    LOOKUP (3):    0.00571s
                          SIZE: 13MB
### BTree ###
                         INDEX:    9.95285s
                    LOOKUP (1):    0.00005s
                    LOOKUP (3):    0.00015s
                          SIZE: 66MB (10 files)
### SQLite3 ###
                         INDEX:    2.90530s
                    LOOKUP (1):    0.11260s
                    LOOKUP (3):    0.11297s
                          SIZE: 25MB
### MongoDB ###
                         INDEX:    14.15286s
                    LOOKUP (1):    0.07876s
                    LOOKUP (3):    0.08125s
                          SIZE: 33MB data, 2.8MB index
```

Obviously nothing can compare to BTree for lookup speed O(log n) compared to O(n), but its insert time is poor and space wise BFI can index 10-30 fields for the same overhead as two BTree fields.  Sub 3ms lookup times for BFI on this dataset is respectable within an application setting - and it should scale linearly which equates to over 45 million records per seconds (but that needs testing...)!

Compared to the database full table scans, BFI is around 50 times faster than SQLite and 30 times faster than a basic MongoDB find({...}).  As with all benchmarks, the comparisons could be optomised but the point of this is to demonstrate the low overhead high performance boost that BFI could give to something like MongoDB.

Another factor that makes a significant storage impact on NoSQL datasets is that the default primary keys are typically larger than 4 or 8 bytes as used by RDBs - Mongo has managed to squeeze it to 12bytes, other DBs use 20byte UUIDs.  When using BTree the primary key has to be stored with every value which makes indexes expensive - and often larger than the values indexed.  BFI stores each primary key only once.  If we use 12 byte primary keys in the above examples it adds another 8MB, while the BFI datastructure would only grow by 0.8MB.

One last point to make is that if we increased this to 30 fields, only the indexing time of BFI would go up (x3), not the storage requirements or query times.  This is ideal for document-orientated databases where developers should be storing all immediately related data in the document rather than normalising.

## Application

See [implementation](implementation.md) first for overview of how it works.

I see this as a index you would turn on per collection with all document (and subdocument) elements automatically added as records are inserted or updated.
Below is an example of the way an indexer should generate strings to pass to BFI.

```json
{
  name: "sue",
  age: 22,
  status: "A",
  groups: ["news", "sport"]
  company: {
    name: "BBC",
    media: ["TV", "radio"]
  }
}
```

```
# plain elements
name:sue
age:22
status:A

# index complex elements as json to allow exact match
groups:["news", "sport"]

# list elements
groups:news
groups:sport

# optionally index positions as well
groups.0:news
groups.1:sport

# recurse into any subdocuments and prefix
company:{name: "BBC", media: ["TV", "radio"]}
company.name:BBC
company.media:["TV", "radio"]
company.media:TV
company.media:radio
```

Ideally it would allow users to specify functions to include extra elements e.g.

```
add_index(function(record) { 
  return ['name:' + record['name'].substring(0, 2) + '*' ];
);
```
Which would also allow two character wildcard searching of last name using the existing index e.g. ``name:su*``

The query optomiser can then use it as it for all AND conditions but results MUST be verified before returning e.g.

```
find( {status: "A", age: {$gt: 18}, groups: sport} )
for pk in bfi.lookup(["status:A", "groups:sport]):
    r = get(pk)
    if r['status'] == "A" and "sport" in r['groups'] and r['age'] > 18:
        yield r
```

## Limitations

* No range queries - though user defined ranges can be used e.g ``age:18-30``
* Probabilistic nature of bloom filters requires validation of results.
* Current implementataion designed to keep probability below 0.001 for up to 30 fields indexed.

