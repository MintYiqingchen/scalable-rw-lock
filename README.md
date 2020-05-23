# How to run
## Run test case with model-checker
```Bash
> mkdir build
> cd build
> cmake -DMODEL_CHK_PATH=${model_checker_root} ..
# example:
> cmake -DMODEL_CHK_PATH=/home/mintyi/codework/model-checker ..
> make
> ./simpleRwlock -v -m 1 -x 100 2 50
> ./snziRwlock -v -m 1 -x 100 2 50
```
## Run test case with pthread
```Bash
> cmake ..
> make
# test from 1-48 threads, 10% write operation
> ./simpleRwlock 48 10 
> ./pthreadRwlock 48 10
> ./snziRwlock 48 10
```

# Lock design
## fairness
This is a lock-free read-write lock giving contention fairness.
The lock class hold 2 atomic variable, `atomic<bool> _exclusive` and `atomic<int> _nreader`,to indicate lock status.
Function `writeLock()` has two steps to guarantee mutual exclusion:
1. spin on `_exclusive` until a successful `CAS(_exclusive, false, true)`. This step guarantees writer-writer exclusion.
2. spin on `_nreader` until `_nreader == 0`. This step guarantees writer-reader exclusion.

Function `writeUnlock()` just simply set `_exclusive = false`, which let other writers and readers have chances to go into critical section.

Function `readLock()` has 2 steps to guarantee mutual exclusion:
1. spin on `_exclusive` until `_exclusive == false`. 
This step guarantees fairness and reader-writer exclusion -- no more readers can increment `_nreader` when there's writer waiting for or holding lock.
It also help avoid extra rmw operations on `_nreader`.
2. update `_nreader ++`. 
However, chances are that between step 1 and step 2, another writer is scheduled in and successfully run `writeLock()`.
So after `_nreader ++`, we have to check whether `_exclusive == false` again. If this check fails, we'll run `_nreader --` to resume `_nreader` to its old value then return to step 1.
Otherwise, the reader get lock successfully.

Function `readUnlock()` just simply decrement `_nreader`.

The simple version of this RW lock is in `simpleRWLock.cpp`. The correctness is verified by test case and CDSChecker.

## scalability
In a read majority scenario, the shared counter `_nreader` is the bottleneck of the read-write lock. 
The `fetch_add(_nreader, 1) and fetch_sub(_nreader, 1)` are called by every reader, which invalidates other readers' cache-line frequently.
Because a single atomic counter will degrade the throughput of the program as #threads going up, 
this implementation (`snziRwlock.cpp`) adopts **SNZI** to replace `_nreader`. Other parts are the same as `simpleRWLock.cpp`.

SNZI is a lock-free tree structure. Since a writer only care about whether `_nreader == 0`, SNZI keeps this information in its root node.
By calling `snzi.query()`, a writer know whether there are readers in the critical section.
With SNZI, `_nreader ++` is replaced as `snzi.arrive()`; `_nreader --` is replaced as `snzi.depart()`.
By default, the SNZI tree is a binary tree with depth = 3 (8 leaves).
### union Node
Every node is at least 64 bytes (the size of most of cache line) to avoid false sharing.
Root node has `atomic<int> indicator` that indicates if #readers > 0 and `atomic<int> val` that is actually (version:16, surplus:15, announce bit:1).
Non-root node only has `atomic<int> val` that is actually (version:16, surplus:16).

### Differences with the paper
My implementation of SNZI is roughly the same as the description in 
[SNZI: Scalable NonZero Indicators](http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.169.6386&rep=rep1&type=pdf).
Here are some modifications:
1. I use `surplus = -1` instead of `surplus = 1/2` to indicate parent in order to encode it in `atomic<int> val` field.
2. Because C++ doesn't support explicit LL/SC operations, in function `root_arrive() and root_depart()` (line 138, line 157), 
two loops and CAS are used to set and unset the leftmost bit of `atomic<int> indicator`.

# Test cases
There are 3 files, `pthreadRWLock.cpp`, `simpleRWLock.cpp` and `snziRwlock.cpp` is provided. 
They are used to do performance comparison.

The logic of the test case is:
1. Create 1 - N threads.
2. Each thread set global variables `a=thread_id` and `b=thread_id`.
3. Each thread read `a` and `b`. And assert `a == b`.

If there are readers and writers in the critical section simultaneously, sometimes `a!=b`.
CDSChecker is used to check data races and deadlock problem. There's no such problem.