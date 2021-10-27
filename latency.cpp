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
#include <inttypes.h>
#include "rng/rng.h"

using namespace std;
using namespace std::chrono;

#ifdef __APPLE__
#define O_DIRECT 0200000
#endif
#define MAP_SYNC 0x080000 /* perform synchronous page faults for the mapping */
#define MAP_SHARED_VALIDATE 0x03    /* share + validate extension flags */
#define CACHELINESIZE (64)
#define NUM_THREADS 5
#define max(a, b) (a>b?a:b)

string pm_file = "/media/pmem0/ke/pm_bench_pm";
string ssd_file = "/home/ke_wang/test/pm_bench_ssd";
uint64_t object_size = 256;
uint64_t alloc_size = ((uint64_t) 8) << 30; // 8GB

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

pthread_t tids[NUM_THREADS];

uint64_t pm_fd, ssd_fd;
char *dram_addr = NULL, *pm_addr = NULL, *ssd_addr = NULL;
uint64_t *start_index = NULL;
uint64_t start_index_len = 1000000;

void init_addr() {
    pm_fd = open(pm_file.c_str(), O_CREAT | O_RDWR, 0644);
    ssd_fd = open(ssd_file.c_str(), O_CREAT | O_RDWR | O_DIRECT, 0644);
#ifdef __linux__
    if (posix_fallocate(pm_fd, 0, alloc_size) < 0){
        puts("pm fallocate fail\n");
        exit(-1);
    }
    if (posix_fallocate(ssd_fd, 0, alloc_size) < 0){
        puts("ssd fallocate fail\n");
        exit(-1);
    }
#endif
    dram_addr = (char *) malloc(alloc_size);
    pm_addr = (char *) mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_SYNC | MAP_SHARED_VALIDATE, pm_fd, 0);
//    ssd_addr = (char *) mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, O_SYNC | MAP_SHARED, ssd_fd, 0);
    close(pm_fd);
//    close(ssd_fd);
}

void free_addr() {
    delete[]dram_addr;
    munmap(pm_addr, alloc_size);
//    munmap(ssd_addr, alloc_size);
    close(ssd_fd);

}

void create_index(uint64_t _size) {
    // set test index length
//    start_index_len = (1 << 24) / _size;
//    start_index_len = max(1024, start_index_len);
    if (_size <= (1 << 10)) {
        start_index_len = (1 << 22) / _size;
    } else {
        start_index_len = (1 << 27) / _size;
    }
    start_index = new uint64_t[start_index_len + 1];
    rng r;
    rng_init(&r, 1, 2);
    uint64_t right_limit = alloc_size - 2 * _size;
    if (right_limit < 0) {
        cout << "Can not create index! Object size too large!\n";
        exit(1);
    }
    for (int i = 0; i < start_index_len; ++i) {
        start_index[i] = rng_next(&r) % right_limit;
        start_index[i] -= start_index[i] % 64;
//        cout << start_index[i] << endl;
    }
}

uint64_t read_dram(uint64_t _size) {
    uint64_t total_delay = 0;
    struct timespec t1, t2;
    uint64_t value = 0;
    for (int i = 0; i < start_index_len; ++i) {
        uint64_t offset = start_index[i];
        clock_gettime(CLOCK_REALTIME, &t1);
        for (int j = 0; j < _size; j += 8) {
            value = *((uint64_t *) (dram_addr + offset + j));
        }
        clock_gettime(CLOCK_REALTIME, &t2);
        total_delay += (uint64_t) (t2.tv_sec - t1.tv_sec) * 1000000000LL + (t2.tv_nsec - t1.tv_nsec);
        clflush(dram_addr + offset, _size);
    }
    return total_delay / start_index_len;
}

uint64_t write_dram(uint64_t _size) {
    uint64_t total_delay = 0;
    struct timespec t1, t2;
    uint64_t value = 1;
    for (int i = 0; i < start_index_len; ++i) {
        uint64_t offset = start_index[i];
        clock_gettime(CLOCK_REALTIME, &t1);
        for (int j = 0; j < _size; j += 8) {
            *((uint64_t *) (dram_addr + offset + j)) = value;
        }
        clock_gettime(CLOCK_REALTIME, &t2);
        total_delay += (uint64_t) (t2.tv_sec - t1.tv_sec) * 1000000000LL + (t2.tv_nsec - t1.tv_nsec);
        clflush(dram_addr + offset, _size);
    }
    return total_delay / start_index_len;
}

uint64_t read_pm(uint64_t _size) {
    uint64_t total_delay = 0;
    __m256d value;
    struct timespec t1, t2;
    for (int i = 0; i < start_index_len; ++i) {
        uint64_t offset = start_index[i];
        clock_gettime(CLOCK_REALTIME, &t1);
        for (int j = 0; j < _size; j += 32) {
            value = _mm256_load_pd((double const *) (pm_addr + offset + j));
        }
        clock_gettime(CLOCK_REALTIME, &t2);
        total_delay += (uint64_t) (t2.tv_sec - t1.tv_sec) * 1000000000LL + (t2.tv_nsec - t1.tv_nsec);
        clflush(pm_addr + offset, _size);
    }
    return total_delay / start_index_len;
}

uint64_t write_pm(uint64_t _size) {
    uint64_t total_delay = 0;
    __m256d write_value;
    double *write_value_addr = (double *) &write_value;
    write_value_addr[0] = 1;
    write_value_addr[1] = 2;
    write_value_addr[2] = 3;
    write_value_addr[3] = 4;

    struct timespec t1, t2;
    for (int i = 0; i < start_index_len; ++i) {
        uint64_t offset = start_index[i];
        clock_gettime(CLOCK_REALTIME, &t1);
        for (int j = 0; j < _size; j += 32) {
            _mm256_stream_pd((double *) (pm_addr + offset + j), write_value);
        }
        clock_gettime(CLOCK_REALTIME, &t2);
        total_delay += (uint64_t) (t2.tv_sec - t1.tv_sec) * 1000000000LL + (t2.tv_nsec - t1.tv_nsec);
        clflush(pm_addr + offset, _size);
    }
    return total_delay / start_index_len;
}

uint64_t read_ssd(uint64_t _size) {
    uint64_t total_delay = 0;
    uint64_t buf_size = max(4096, _size);
    unsigned char *buf;
    posix_memalign((void **) &buf, 512, buf_size);
    struct timespec t1, t2;
    for (int i = 0; i < start_index_len; ++i) {
        uint64_t offset = start_index[i];
        clock_gettime(CLOCK_REALTIME, &t1);
        lseek(ssd_fd, offset, SEEK_SET);
        read(ssd_fd, buf, buf_size);
        clock_gettime(CLOCK_REALTIME, &t2);
        total_delay += (uint64_t) (t2.tv_sec - t1.tv_sec) * 1000000000LL + (t2.tv_nsec - t1.tv_nsec);
    }
    return total_delay / start_index_len;
}

uint64_t write_ssd(uint64_t _size) {
    uint64_t total_delay = 0;
    uint64_t buf_size = max(4096, _size);
    unsigned char *buf;
    posix_memalign((void **) &buf, 512, buf_size);
    for (int i = 0; i < buf_size; ++i) {
        buf[i] = 'a' + i % 26;
    }
    struct timespec t1, t2;
    for (int i = 0; i < start_index_len; ++i) {
        uint64_t offset = start_index[i];
        clock_gettime(CLOCK_REALTIME, &t1);
        lseek(ssd_fd, offset, SEEK_SET);
        write(ssd_fd, buf, buf_size);
        clock_gettime(CLOCK_REALTIME, &t2);
        total_delay += (uint64_t) (t2.tv_sec - t1.tv_sec) * 1000000000LL + (t2.tv_nsec - t1.tv_nsec);
    }
    return total_delay / start_index_len;
}


void benchmark(uint64_t _size) {
    uint64_t read_dram_latency = 0, write_dram_latency = 0;
    uint64_t read_pm_latency = 0, write_pm_latency = 0;
    uint64_t read_ssd_latency = 0, write_ssd_latency = 0;

    create_index(_size);

    write_dram_latency = write_dram(_size);
    read_dram_latency = read_dram(_size);
    write_pm_latency = write_pm(_size);
    read_pm_latency = read_pm(_size);
    write_ssd_latency = write_ssd(_size);
    read_ssd_latency = read_ssd(_size);

    cout << read_dram_latency << ", " << write_dram_latency << ", " << read_pm_latency << ", " << write_pm_latency
         << ", " << read_ssd_latency << ", " << write_ssd_latency << endl;
}

int main(int argc, char *argv[]) {
    sscanf(argv[1], "%" SCNu64, &object_size);
//    sscanf(argv[2], "%" SCNu64 , &alloc_size);
    init_addr();
    disable_cache();
    benchmark(object_size);
    free_addr();
    return 0;
}