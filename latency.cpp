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

using namespace std;
using namespace std::chrono;

#define MAP_SYNC 0x080000 /* perform synchronous page faults for the mapping */
#define MAP_SHARED_VALIDATE 0x03    /* share + validate extension flags */
#define ALLOC_SIZE ((size_t)4<<30) // 4GB
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

pthread_t tids[NUM_THREADS];

string pm_file = "/media/pmem0/pm_bench_pm";
string ssd_file = "/home/ke_wang/test/pm_bench_ssd";

char *dram_addr = NULL, *pm_addr = NULL, *ssd_addr = NULL;

uint64_t size = 1 << 30; //1GB
uint64_t cache_line_size = 64; //64Byte
uint64_t len = size / sizeof(uint64_t);

void read_dram(int _size) {

}

void init_addr() {
    int pm_fd = open(pm_file.c_str(), O_CREAT | O_RDWR, 0644);
    int ssd_fd = open(ssd_file.c_str(), O_CREAT | O_RDWR, 0644);
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
    dram_addr = (char *)malloc(ALLOC_SIZE);
    pm_addr = (char *) mmap(NULL, ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_SYNC | MAP_SHARED_VALIDATE, pm_fd, 0);
    ssd_addr = (char *) mmap(NULL, ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ssd_fd, 0);
    close(pm_fd);
    close(ssd_fd);
}

int main() {
    init_addr();

    return 0;
}