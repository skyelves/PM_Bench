#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <x86intrin.h>
#include <immintrin.h>
#include <chrono>
#include <iostream>
#include <pthread.h>
#include <sys/timeb.h>
#include "rng/rng.h"

using namespace std;
using namespace std::chrono;

#define MAP_SYNC 0x080000 /* perform synchronous page faults for the mapping */
#define MAP_SHARED_VALIDATE 0x03    /* share + validate extension flags */
#define ALLOC_SIZE ((size_t)4<<30) // 4GB
#define CACHELINESIZE (64)
#define NUM_THREADS 5

#define Time_BODY(condition, name, func)                                                        \
    if(condition) {                                                                             \
        sleep(1);                                                                               \
        timeval start, ends;                                                                    \
        gettimeofday(&start, NULL);                                                             \
        func                                                                                    \
        gettimeofday(&ends, NULL);                                                              \
        double timeCost = (ends.tv_sec - start.tv_sec) * 1000000 + ends.tv_usec - start.tv_usec;\
        double throughPut = (double) testNum / timeCost;                                        \
        cout << throughPut << ", " << endl;                                                     \
    }

inline void mfence(void) {
    asm volatile("mfence":: :"memory");
}

inline void clflush(char *data, size_t len) {
    volatile char *ptr = (char *) ((unsigned long) data & (~(CACHELINESIZE - 1)));
    mfence();
    for (; ptr < data + len; ptr += CACHELINESIZE) {
        asm volatile("clflush %0" : "+m" (*(volatile char *) ptr));
    }
    mfence();
}

string pm_file = "/media/pmem0/ke/pm_bench_pm";
string ssd_file = "/home/ke_wang/test/pm_bench_ssd";
uint64_t object_size = 64;

pthread_t tids[NUM_THREADS];

char *dram_addr = NULL, *pm_addr = NULL, *ssd_addr = NULL, *content = NULL;
uint64_t *start_index = NULL;
uint64_t start_index_len = 1000000;

void init_addr() {
    uint64_t pm_fd = open(pm_file.c_str(), O_CREAT | O_RDWR, 0644);
    uint64_t ssd_fd = open(ssd_file.c_str(), O_CREAT | O_RDWR, 0644);
#ifdef __linux__
    if (posix_fallocate(pm_fd, 0, ALLOC_SIZE) < 0){
        puts("pm fallocate fail\n");
        exit(-1);
    }
    if (posix_fallocate(ssd_fd, 0, ALLOC_SIZE) < 0){
        puts("ssd fallocate fail\n");
        exit(-1);
    }
#endif
    dram_addr = (char *) malloc(ALLOC_SIZE);
    pm_addr = (char *) mmap(NULL, ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_SYNC | MAP_SHARED_VALIDATE, pm_fd, 0);
    ssd_addr = (char *) mmap(NULL, ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ssd_fd, 0);
    close(pm_fd);
    close(ssd_fd);
}

void create_index() {
    start_index = new uint64_t[start_index_len];
    rng r;
    rng_init(&r, 1, 2);
    uint64_t right_limit = ALLOC_SIZE - 2 * object_size;
    for (int i = 0; i < start_index_len; ++i) {
        start_index[i] = rng_next(&r) % right_limit;
    }
}

uint64_t read_dram(uint64_t _size) {

}

uint64_t write_dram(uint64_t _size) {
    uint64_t total_delay = 0;
    struct timespec t1, t2;
    for (int i = 0; i < start_index_len; ++i) {
        uint64_t offset = start_index[i];
        clock_gettime(CLOCK_REALTIME, &t1);
        strcpy(dram_addr + offset, content);
        clock_gettime(CLOCK_REALTIME, &t2);
        total_delay += (uint64_t) (t2.tv_sec - t1.tv_sec) * 1000000000LL + (t2.tv_nsec - t1.tv_nsec);
        clflush(dram_addr + offset, object_size);
    }
    return total_delay / start_index_len;
}


void benchmark() {
    uint64_t read_dram_latency = 0, write_dram_latency = 0;
    uint64_t read_pm_latency = 0, write_pm_latency = 0;
    uint64_t read_ssd_latency = 0, write_ssd_latency = 0;
    content = new char[object_size + 1];
    for (int i = 0; i < object_size; ++i) {
        content[i] = 'a' + (i % 26);
    }
    content[object_size - 1] = '\0';
    create_index();

    write_dram_latency = write_dram(object_size);

    cout << write_dram_latency << endl;
}

int main() {
    init_addr();
    benchmark();

    return 0;
}