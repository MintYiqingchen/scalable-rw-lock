#include <iostream>
#include <chrono>
#include <pthread.h>
#include <vector>
constexpr int ITERATION = 100000000;
int a = 0, b = 0, interval = 10000;
std::vector<int> Ids;
class SimpleRWLock {
    pthread_rwlock_t _rwlock = PTHREAD_RWLOCK_INITIALIZER;
public:
    void writeLock() {
        if(pthread_rwlock_wrlock(&_rwlock) != 0){
            std::cerr << "wr lock error!\n";
        }
    }
    void writeUnlock() {
        if(pthread_rwlock_unlock(&_rwlock)) {
            std::cerr << "wr unlock error!\n";
        }
    }
    void readLock() {
        if(pthread_rwlock_rdlock(&_rwlock)) {
            std::cerr << "rd lock error!\n";
        }
    }
    void readUnlock() {
        if(pthread_rwlock_unlock(&_rwlock)) {
            std::cerr << "rd unlock error!\n";
        }
    }
};
SimpleRWLock rwLock;

void* worker(void* param) {
    int id = *(int*)param;
    for(int i = 0; i < ITERATION; ++ i) {
        if((i % interval) == 0){
            rwLock.writeLock();
            a = id;
            b = id;
            rwLock.writeUnlock();
        }
        else {
            rwLock.readLock();
            int ta = a, tb = b;
            if(ta != tb) {
                std::cerr << "[ERROR] " << ta << " != " << tb << std::endl;
            }
            rwLock.readUnlock();
        }
    }
}

void test(int threadN, int interval, double wPerc) {
    using namespace std::chrono;
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
}
int main(int argc, char** argv) {
    if(argc < 3) {
        std::cout << "usage: ./pthreadRwlock [thread number] [write percent]" << std::endl;
    }
    int threadNum = atoi(argv[1]);
    double wPerc = atof(argv[2]);
    std::cout << "thread number: "<<threadNum << ", write percent: ";
    std::cout.precision(2);
    std::cout << wPerc << "%\n";

    int interval;
    if(wPerc == 0)
        interval = ITERATION;
    else
        interval = ITERATION / (int)(wPerc * 0.01 * ITERATION);
    // std::cout << interval << std::endl;
    Ids.resize(threadNum);
    for(int i = 0; i < threadNum; ++ i) {Ids[i] = i + 1;}
    test(threadNum, interval, wPerc);
    return 0;
}
