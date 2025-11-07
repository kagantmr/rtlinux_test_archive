
#define _POSIX_C_SOURCE 200809L
#include "lib/merge_sort.h"
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <sched.h>

#include <stdio.h>

#define PERIOD_NS 10000000L  // 10 ms
#define SIZE 50


static volatile sig_atomic_t running = 1;

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}



static int merge_buffer[SIZE]; // global buffer

void merge(int* array, size_t start, size_t middle, size_t end) {
    size_t i, j, k;

    i = start;
    j = middle + 1;
    k = start;

    while (i <= middle && j <= end) {
        if (array[i] <= array[j]) {
            merge_buffer[k++] = array[i];
        } else {
            merge_buffer[k++] = array[j];
        }
    }
    
    while (i <= middle) { 
        merge_buffer[k++] = array[i++]; 
    }

    while (j <= end) { 
        merge_buffer[k++] = array[j++]; 
    }

    for (int i = start; i < end; i++) {
        array[i] = merge_buffer[i];
    }

}

void merge_sort(int* array, size_t start, size_t end) {
    if (start < end) {
        size_t middle = (start + end) / 2;
        merge_sort(array, start, middle);
        merge_sort(array, middle+1, end);
        merge(array, start, middle, end);
    }
} 

static unsigned int x = 123456789, y = 362436069, z = 521288629;
static inline unsigned int xor_random(void) {
    unsigned int t = x ^ (x << 11);
    x = y; y = z;
    z ^= (z >> 19) ^ (t ^ (t >> 8));
    return z & 1023;
}

void fill_array(int *array) {
    for (int i = 0; i < SIZE; i++)
        array[i] = xor_random();
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    int *arr = calloc(SIZE, sizeof(int));
    if (!arr) {
        perror("calloc");
        return EXIT_FAILURE;
    }

    // setup realtime environment
    struct sched_param p;
    p.sched_priority = 80;
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("mlockall");
        return EXIT_FAILURE;
    }
    if (sched_setscheduler(0, SCHED_FIFO, &p) == -1) {
        perror("sched_setscheduler");
        munlockall();
        return EXIT_FAILURE;
    }

    struct timespec start, end, next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    while (running) {
        fill_array(arr);

        clock_gettime(CLOCK_MONOTONIC, &start);
        merge_sort(arr, 0, SIZE - 1);
        clock_gettime(CLOCK_MONOTONIC, &end);

        next.tv_nsec += PERIOD_NS;
        while (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec++;
        }

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    munlockall();
    free(arr);
    return EXIT_SUCCESS;
}