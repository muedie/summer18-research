## Summer 2018 Research - Cache Replacement Algorithms

lru folder contains code to run the lru algorithm

To compile: `g++ lru.cpp`

To run: `a.out <cache-size> <tracefile>`

output line: `Access: <# of accesses> Hits: <# of hits> Miss: <# of misses>`

the subfolder lru_exports contains png of some runs
on traces inside traces/ folder

lirs folder contains code to run the lirs algorithm

To compile: `gcc lirs.c`

To run: `a.out tracefile <data-points def: 10> <HIR rate def: 1>`

output line: `HIR: <hir rate> <cache-size> <miss ratio>`

data-points work by runnning n times from 0 to k cache size
in each k/n interval

reports folder contain outputs of various algorithms combined
usually follows (algo)_(tracefile)_(hir)?.dat filename structure
and file contains lines and each line is (cache-size) (hit ratio)

traces folder contain tracefile to test with previous algorithms