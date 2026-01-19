#define _GNU_SOURCE
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <sched.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define PERIOD_NS 10000000L  // 10 ms
#define SIZE 64
#define STACK_PREFLT (4 * 1024 * 1024)

static volatile sig_atomic_t running = 1;

// Statically allocated buffers
static int merge_buffer[SIZE];

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

void prefault_stack(void) {
    volatile char stack[STACK_PREFLT];
    memset((void*)stack, 0, STACK_PREFLT);
}

// Helper: Standard merge logic
static void merge(int *array, int start, int middle, int end) {
    int i = start, j = middle + 1, k = start;

    while (i <= middle && j <= end) {
        if (array[i] <= array[j])
            merge_buffer[k++] = array[i++];
        else
            merge_buffer[k++] = array[j++];
    }

    while (i <= middle)
        merge_buffer[k++] = array[i++];
    while (j <= end)
        merge_buffer[k++] = array[j++];

    for (int n = start; n <= end; n++)
        array[n] = merge_buffer[n];
}

// RT-SAFE: Iterative Merge Sort (No Recursion)
void merge_sort_iterative(int *arr, int n) {
    int curr_size;  // Size of subarrays: 1, 2, 4, 8...
    int left_start; // Start index of left subarray

    for (curr_size = 1; curr_size <= n - 1; curr_size = 2 * curr_size) {
        for (left_start = 0; left_start < n - 1; left_start += 2 * curr_size) {
            
            int mid = left_start + curr_size - 1;
            int right_end = left_start + 2 * curr_size - 1;

            if (mid >= n) mid = n - 1;
            if (right_end >= n) right_end = n - 1;

            merge(arr, left_start, mid, right_end);
        }
    }
}

// RNG
static unsigned int x = 123456789, y = 362436069, z = 521288629;
static inline unsigned int xor_random(void) {
    unsigned int t = x ^ (x << 11);
    x = y; y = z;
    z ^= (z >> 19) ^ (t ^ (t >> 8));
    return z & 1023;
}

static void fill_array(int *array) {
    for (int i = 0; i < SIZE; i++)
        array[i] = xor_random();
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    // Statically allocate array or use stack if small enough
    // For RT, malloc is okay in initialization (Main thread), but static is safer
    int *arr = calloc(SIZE, sizeof(int));
    if (!arr) return EXIT_FAILURE;

    struct sched_param p;
    p.sched_priority = 80;

    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("mlockall"); return EXIT_FAILURE;
    }
    prefault_stack();

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(2, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

    if (sched_setscheduler(0, SCHED_FIFO, &p) == -1) {
        perror("sched_setscheduler"); return EXIT_FAILURE;
    }

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    printf("RT Sort: Running Iterative Merge Sort Loop...\n");

    while (running) {
        // 1. New Random Data
        fill_array(arr);

        // 2. Sort (Iterative)
        merge_sort_iterative(arr, SIZE);

        // 3. Sleep
        next.tv_nsec += PERIOD_NS;
        while (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    munlockall();
    free(arr);
    printf("RT Sort: Finished.\n");
    return EXIT_SUCCESS;
}