# Bloom Filter Index

**Alpha code - do not use in production**

## Overview

An experimental new index designed for document-based databases using bloom filters.  Allows for fast full table scans across **ALL** fields without needing
to deserialize.

# Features

* Indexing of **ALL** record fields, including sub-elements.
* Linear time regardless of number of fields searched for.
* Simple add, remove, update - no tree to rebalance.
* Compact structure compared to multiple btrees.
* Low CPU usage.

# Performance

Early days but showing promise.  On a dataset of 470,000 items with 10 fields indexed. 

|                       | BFI           | BSDDB btree  |
------------------------|---------------|---------------
| Index time            | 11.4s         | ?            |
| Size:                 | 60MB          | 12-18MB * 10 |
| Query time (1 item)   | 0.03s         | 0.03s        |
| Query time (3 items)  | 0.03s         | 0.31s        |

Note that the BSDDB timings are run via python so you can probably knock a little off the lookup times for that.

# Limitations

* No range queries - though user defined ranges can be used e.g ``AgeRange:18-30``
* Probabilistic nature of bloom filters requires validation of results.

# TODO

* Implement mmap for lookup - should give significant boost
* Get someone who can actually write C to go over the code.