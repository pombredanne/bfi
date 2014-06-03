# Bloom Filter Index Implemetation

## Why

Relational databases are incredibly efficient - in some ways too efficient!  People have got used to being able to run complex queries against unindexed data and have the results back almost instantly.  Having your data nicely aligned means that you can scan a huge table efficiently - particularly now we have fast solid state disks and tonnes of memory.

NoSQL has come to the party and for all its benefits (and there are many), full table scans are not one of them.  The data is stored in a serialized (and often compressed) form so every record must be extracted before being checked which is CPU intensive and slow.

As developers we are told to ensure we index anything we will need in real-time and write map-reduce functions to trawl across the data while we have a tea break.  Unfortunately the large primary key size for most implementations means that for every index we build, a full set of primary keys must be stored along side the values meaning the storage required for the indexes quickly swells, often to many times the size of the original data.

## Basic idea

Bloom filters have a long history of use by large-scale databases as a means to avoid expensive lookups in an index if you **know** it isn't there.  Why not apply the same principle to avoid deserializing objects.

Given the following object:

```json
{
  'first_name': 'Eric',
  'last_name': 'Wimp',
  'profession': 'Superhero',
  'age': 34,
  'address': '29 Acacia Road',
  'town': 'NuttyTown',
  'country': 'UK',
  'likes': ['bananas', 'crows', 'yellow'],
  'dislikes': ['scotsmen', 'fat cats'],
}
```

we create a small bloom filter with a capacity of around 30 elements and feed it all the fields concatinated with their values:

    first_name:Eric
    last_name:Wimp
    profession:Superhero
    age:34
    ...
    likes:bananas
    likes:crows
    likes:yellow
    dislikes:scotsmen
    dislikes:fat cats

We then store the resulting bloom filter with the pk.

To run a query you create another filter with only the fields you are interested in

    profession:Superhero
    country:UK
    likes:yellow

Then quickly scan the data doing a simple bitwise AND of the filters and record the matches.  Note that a match only indicates a good probability (>0.999) that the record fulfills the criteria, **it must be checked before returning to client**.

The main limitation of this technique is that it only supports **exact** matches and can't do ranges.  We can migate this slightly by taking advantage of the fact that we can store many more items, so we could store the additional values:

    first_name:E*
    first_name:Er*
    last_name:W*
    last_name:Wi*
    age:30-39

which would allow us to do one or two letter searches for the names and use predefined age ranges.

## Optomisations

The bloom filter used is optomised for small size and rapid calculation.  It uses 1024 bits with 4 256 bit segments.  The bits to be set are derived from a single run of the murmur3 hash making it very fast to generate.  If i have done the calculations correctly it results in the following error rates and false positives per million records:

Fields indexed | Error rate | fp/m
-------------- | ---------- | ----
10             | 2.15e-6    | 2
20             | 3.19e-5    | 32
30             | 1.49e-4    | 149
40             | 4.38e-4    | 438

These values seem ideal for around 20 fields which is typical for a document.

V1 stored the data in a very simple [pk1, f1], [pk2, f2] ... file format which worked efficiently and gave sub 100ms lookups but involved checking every single bloom filter.

I investigated [bloofi](http://dl.acm.org/citation.cfm?doid=2501928.2501931) as an option but the added complexity and storage requirements made it not an option - unless the data is efficiently sorted to keep similar filters in the same sections of the tree you end up following many paths before discounting them.

We can however use the properties of the filters to get about a 8x increase in query performance. 

The indexes are designed to be saturated - ie as large a number of bits set as possible while remaining within our error guarantees.  This means they dont compress well (they are already, in effect compressed representations of the data).

When we do a query though the filter is sparse - for a single criteria only 4 bits of 1024 will be set.

We can use this to skip large chunks of the index.  The index data is stored in pages with the 128 filter bytes stored in column order.

    Primary keys:   pk1   pk2   pk3   pk4
                    -----------------------
    Filters:        f1[0] f2[0] f3[0] f4[0]
                    f1[1] f2[1] f3[1] f4[1]
                    f1[2] f2[2] f3[2] f4[2]
                    ...
                    f1[n] f2[n] f3[n] f4[n]

Given that the first x bytes of the query filter are 0, we can skip rows 0-x. When the query byte is non zero we just check that row and discount any filters that dont AND with the query byte.

We still have to process the entire datastructure but at least 124 of the 128 rows can be skipped without checking. As the number of criteria increases as does the number of rows that need to be checked per page, up to a worst case similar to version 1.

Inplace updating and deletion of items is straight forward as well as insertion with no need to rebalance.

## Implementation

This is currently **VERY BAD C** - I need someone to help me clean it up!
