#define _GNU_SOURCE
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <sched.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define PERIOD_NS 10000000L  // 10 ms
#define SIZE 32
#define MAX_ITERATIONS 32
#define STACK_PREFLT (4 * 1024 * 1024)

static volatile sig_atomic_t running = 1;

// Global buffers to avoid stack allocation
float A[SIZE][SIZE];
float eigenvalue;
float next_eigenvector[SIZE];
float eigenvector[SIZE] = {0.0f};

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

void prefault_stack(void) {
    volatile char stack[STACK_PREFLT];
    memset((void*)stack, 0, STACK_PREFLT);
}

/* --- MATH HELPERS --- */
static void init_matrix(float matrix[SIZE][SIZE]) {
    for (int i = 0; i < SIZE; ++i) {
        for (int j = 0; j < SIZE; ++j) {
            if (i == j) matrix[i][j] = 2.0f;
            else if (j == i - 1 || j == i + 1) matrix[i][j] = -1.0f;
            else matrix[i][j] = 0.0f;
        }
    }
}

static float arrmax(float v[SIZE]) {
    float largest = 0.0f;
    for (int i = 0; i < SIZE; i++) {
        float a = fabsf(v[i]);
        if (a > largest) largest = a;
    }
    return largest;
}

static void arrcpy(float src[SIZE], float dest[SIZE]) {
    for (int i = 0; i < SIZE; i++) dest[i] = src[i];
}

void normalize(float v[SIZE]) {
    float max = arrmax(v);
    if (max == 0.0f) return;
    for (int i = 0; i < SIZE; i++) v[i] /= max;
}

float dot_product(float v1[SIZE], float v2[SIZE]) {
    float result = 0.0f;
    for (int i = 0; i < SIZE; i++) result += v1[i] * v2[i];
    return result;
}

void matvec_mul(float A[SIZE][SIZE], float v[SIZE], float result[SIZE]) {
    for (int i = 0; i < SIZE; i++) {
        float sum = 0.0f;
        for (int j = 0; j < SIZE; j++) sum += A[i][j] * v[j];
        result[i] = sum;
    }
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    // RT Setup
    struct sched_param p;
    p.sched_priority = 88;

    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("mlockall"); return EXIT_FAILURE;
    }
    prefault_stack();

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);

    if (sched_setscheduler(0, SCHED_FIFO, &p) == -1) {
        perror("sched_setscheduler"); return EXIT_FAILURE;
    }

    // Initialize Data
    init_matrix(A);
    eigenvector[0] = 1.0f;

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    printf("RT Eigenvalue: Running power iteration loop...\n");

    while (running) {
        // --- WORKLOAD ---
        for (int ITER = 0; ITER < MAX_ITERATIONS; ITER++) {
            matvec_mul(A, eigenvector, next_eigenvector);
            float denom = dot_product(eigenvector, eigenvector);
            if (denom < 1e-12f) denom = 1e-12f;
            eigenvalue = dot_product(eigenvector, next_eigenvector) / denom;
            normalize(next_eigenvector);
            arrcpy(next_eigenvector, eigenvector);
        }

        // --- PERIODIC SLEEP ---
        next.tv_nsec += PERIOD_NS;
        while (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    munlockall();
    printf("RT Eigenvalue: Finished.\n");
    return EXIT_SUCCESS;
}