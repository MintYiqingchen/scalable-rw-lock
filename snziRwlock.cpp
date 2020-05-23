//
// Created by mintyi on 5/21/20.
//
#include <iostream>
#include <chrono>
#include <vector>
#include <atomic>
#ifdef WITH_CDS
#include <threads.h>
constexpr int ITERATION = 10;
#else
#include <pthread.h>
constexpr int ITERATION = 100000000;
#endif
#define CACHELINE 64
#define DEPTH 3
int a = 0, b = 0, interval = 10000;
std::vector<int> Ids;

union Node {
    char pad[CACHELINE];
    std::atomic_int val; // (version:16, count:16)
    struct {
        std::atomic_int val; // (version: 16, count: 15, a: 1)
        std::atomic_int indicator;
    } root;
};

class SNZI {
    int nLeaf, nNodes;
    Node* nodes;
public:
    SNZI(int depth = DEPTH) {
        nNodes = (1 << (depth + 1)) - 1;
        nLeaf = 1 << depth;
        nodes = new Node[nNodes];
    }
    ~SNZI() {
        delete [] nodes;
    }
    void arrive(int tid) {
        int idx = nNodes - nLeaf + (tid % nLeaf);
        node_arrive(idx);
    }
    void depart(int tid) {
        int idx = nNodes - nLeaf + (tid % nLeaf);
        node_depart(idx);
    }
    bool query() {
        auto i = nodes[0].root.indicator.load();
        return (i & 0x80000000) > 0;
    }

private:
    void node_arrive(int idx){
        if(idx == 0){
            return root_arrive();
        }
        int undoArr = 0;
        bool succ = false;
        int par_idx = (idx - 1) / 2; // parent's index
        while(!succ) {
            int val = nodes[idx].val.load(), new_val;
            short version, count;
            node_decode(val, version, count);
            if(count >= 1) {
                new_val = node_encode(version, count+1);
                // set to the new value
                if(nodes[idx].val.compare_exchange_strong(val, new_val))
                    break;
            }
            if(count == 0) { // need to invoke parent.arrive
                new_val = node_encode(version+1, -1);
                if(nodes[idx].val.compare_exchange_strong(val, new_val)){
                    succ = true;
                    version ++;
                    count = -1;
                    val = new_val;
                }
            }
            if(count == -1) { // change from -1 to 1 (half completed helper)
                new_val = node_encode(version, 1);
                if(undoArr == 2) { // try to offset one undo
                    if(nodes[idx].val.compare_exchange_strong(
                            val, new_val))
                        undoArr --;
                } else { // help
                    par_idx == 0 ? root_arrive() : node_arrive(par_idx);
                    if(!nodes[idx].val.compare_exchange_strong(val, new_val))
                        undoArr ++;
                }
            }
        }
        while(undoArr --) {
            par_idx == 0 ? root_depart() : node_depart(par_idx);
        }
    }
    void node_depart(int idx) {
        if(idx == 0)
            return root_depart();

        int par_idx = (idx - 1) / 2;
        while(true) {
            int val = nodes[idx].val.load();
            short version, count;
            node_decode(val, version, count);
            if(nodes[idx].val.compare_exchange_strong(val, node_encode(version, count-1))){
                if(count == 1) {
                    par_idx == 0 ? root_depart() : node_depart(par_idx);
                }
                break;
            }
        }
    }
    void root_arrive() {
        int val, new_val;
        short version, count;
        bool a;
        while(true) {
            val = nodes[0].val.load();
            root_decode(val, version, count, a);
            if(count == 0)
                new_val = root_encode(version + 1, 1, true);
            else
                new_val = root_encode(version, count + 1, a);
            if(nodes[0].val.compare_exchange_strong(val, new_val)) {
                break;
            }
        }
        root_decode(new_val, version, count, a);
        if(a) { // update indicator
            do {
                int i = nodes[0].root.indicator.load();
                int newi = (i & 0x7FFFFFFF) + 1;
                newi = (int)(((unsigned int)newi) | 0x80000000);
                if(nodes[0].root.indicator.compare_exchange_strong(i, newi))
                    break;
            }while(1);
            nodes[0].root.val.compare_exchange_strong(new_val, root_encode(version, count, false));
        }
    }
    void root_depart() {
        int val, new_val;
        short version, count;
        bool a;
        while (true) {
            val = nodes[0].root.val.load();
            root_decode(val, version, count, a);
            if(nodes[0].val.compare_exchange_strong(val,
                    root_encode(version, count - 1, false))) {
                if(count >= 2) return;
                while(true) {
                    int i = nodes[0].root.indicator.load();
                    val = nodes[0].root.val.load();
                    if((short)(val >> 16) != version)
                        return; // new version occurs: don't overwrite
                    int newi = (i & 0x7fffffff) + 1;
                    if(nodes[0].root.indicator.compare_exchange_strong(i, newi))
                        return;
                }
            }
        }
    }
    static void node_decode(int val, short& version, short& count) {
        version = (val >> 16);
        count = val & 0x7FFF;
    }
    static int node_encode(short version, short count) {
        int res = version;
        res <<= 16;
        return res + count;
    }
    static void root_decode(int val, short& version, short& count, bool& a) {
        version = (val >> 16);
        a = (val & 1) == 1;
        count = ((val & 0xFFFF) >> 1) & 0x7fff;
    }
    static int root_encode(short version, short count, bool a) {
        int res = version, tmp = count;
        res <<= 16;
        res += (tmp << 1) + (int)a;
        return res;
    }
};

class SNZIRWLock {
    std::atomic<bool> _exclusive;
    SNZI snzi;
public:
    SNZIRWLock() {
        _exclusive = false;
    }
    void writeLock() {
        bool localEx = false;
        // writer-writer mutual exclusion
        while(_exclusive.compare_exchange_strong(localEx, true,
                                                 std::memory_order_acq_rel)) {
            localEx = false;
        }
        // writer-reader mutual exclusion
        while(snzi.query()){}
    }
    void writeUnlock() {
        _exclusive.store(false);
    }
    void readLock(int tid) {
        while(1) {
            // reader-writer mutual exclusion
            while (_exclusive.load()) {}
            // update reader
            snzi.arrive(tid);
            // reader-writer mutual exclusion
            if (!_exclusive.load()) break;
            snzi.depart(tid);
        }
    }
    void readUnlock(int tid) {
        // update reader
        snzi.depart(tid);
    }
};
SNZIRWLock* rwLockptr;

#ifdef WITH_CDS
void worker(void* param) {
#else
void* worker(void* param) {
#endif
    SNZIRWLock& rwLock = *rwLockptr;
    int id = *(int*)param;
    for(int i = 0; i < ITERATION; ++ i) {
        if((i % interval) == 0){
            rwLock.writeLock();
            a = id;
            b = id;
            rwLock.writeUnlock();
        }
        else {
            rwLock.readLock(id);
            int ta = a, tb = b;
            if(ta != tb) {
                std::cerr << "[ERROR] " << ta << " != " << tb << std::endl;
            }
            rwLock.readUnlock(id);
        }
    }
    return NULL;
}

void test(int threadN, int interval, double wPerc) {
    using namespace std::chrono;
#ifdef WITH_CDS
    std::vector<thrd_t> tid(threadN);
    for(int j = 1; j <= threadN; ++ j){
        thrd_create(&tid[j-1], worker, &Ids[j-1]);
    }
    for(int j = 1; j <= threadN; ++ j) {
        thrd_join(tid[j-1]);
    }
#else
    std::vector<pthread_t> tid(threadN);
    for(int i = 1; i <= threadN; ++ i) {
        auto start = system_clock::now();
        for(int j = 1; j <= i; ++ j){
            pthread_create(&tid[j-1], NULL, worker, &Ids[j-1]);
        }
        for(int j = 1; j <= i; ++ j) {
            pthread_join(tid[j-1], NULL);
        }
        auto end = system_clock::now();
        auto duration = duration_cast<milliseconds>(end - start);
        auto thoughput = ITERATION / (duration.count() * 1.0) * i;
        std::cout << i <<"," << thoughput <<","<< wPerc << std::endl;
    }
#endif
}

#ifdef WITH_CDS
int user_main(int argc, char **argv){
#else
int main(int argc, char** argv) {
#endif
    if(argc < 3) {
        std::cout << "usage: ./simpleRwlock [thread number] [write percent]" << std::endl;
        exit(0);
    }
    rwLockptr = new SNZIRWLock();
    int threadNum = atoi(argv[1]);
    double wPerc = atof(argv[2]);
    std::cout << "thread number: "<<threadNum << ", write percent: ";
    std::cout.precision(2);
    std::cout << wPerc << "%\n";

    int interval;
    if(wPerc == 0)
        interval = ITERATION + 1;
    else
        interval = ITERATION / (int)(wPerc * 0.01 * ITERATION);
    // std::cout << interval << std::endl;
    Ids.resize(threadNum);
    for(int i = 0; i < threadNum; ++ i) {Ids[i] = i + 1;}
    test(threadNum, interval, wPerc);
    return 0;
}
