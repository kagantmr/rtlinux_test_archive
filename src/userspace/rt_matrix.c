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
#define SIZE 32
#define STACK_PREFLT (4 * 1024 * 1024)

static volatile sig_atomic_t running = 1;

int A[SIZE][SIZE];
int B[SIZE][SIZE];
int result[SIZE][SIZE];

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

void prefault_stack(void) {
    volatile char stack[STACK_PREFLT];
    memset((void*)stack, 0, STACK_PREFLT);
}

static void fill_matrix(int m[SIZE][SIZE]) {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            m[i][j] = i + j;
        }
    }
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    fill_matrix(A);
    fill_matrix(B);

    struct sched_param p;
    p.sched_priority = 85;

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

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    printf("RT Matrix: Running Matrix Mul Loop...\n");

    while (running) {
        // --- WORKLOAD ---
        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                int sum = 0;
                for (int k = 0; k < SIZE; k++) {
                    sum += A[i][k] * B[k][j];
                }
                result[i][j] = sum;
            }
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
    printf("RT Matrix: Finished.\n");
    return EXIT_SUCCESS;
}