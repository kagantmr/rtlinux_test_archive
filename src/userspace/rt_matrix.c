#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <sched.h>
#include <stdio.h>

#define PERIOD_NS 10000000L  // 10 ms
#define SIZE 32
#define LOG_SIZE 10000       // number of timing samples

static volatile sig_atomic_t running = 1;
static long long latencies[LOG_SIZE];
static int latency_index = 0;

// allocate matrices statically to prevent stack overflow
int A[SIZE][SIZE];
int B[SIZE][SIZE];
int result[SIZE][SIZE];

// Fill a matrix with sum of indices
static void fill_matrix(int m[SIZE][SIZE]) {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            m[i][j] = i + j;
        }
    }
}

void matmul(int A[SIZE][SIZE], int B[SIZE][SIZE], int result[SIZE][SIZE]) {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            int sum = 0;
            for (int k = 0; k < SIZE; k++) {
                sum += A[i][k] * B[k][j];
            }
            result[i][j] = sum;
        }
    }
}

/* ---------- Signal handling ---------- */
void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

/* ---------- main ---------- */
int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    fill_matrix(A);
    fill_matrix(B);

    /* Setup realtime environment */
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

    /* ---------- Real-time Loop ---------- */

    
    while (running) {

        clock_gettime(CLOCK_MONOTONIC, &start);
        matmul(A, B, result);
        clock_gettime(CLOCK_MONOTONIC, &end);

        long long elapsed =
            (end.tv_sec - start.tv_sec) * 1000000000LL +
            (end.tv_nsec - start.tv_nsec);

        if (latency_index < LOG_SIZE)
            latencies[latency_index++] = elapsed;

        next.tv_nsec += PERIOD_NS;
        while (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec++;
        }

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    // ---------- Write logged timings to file after loop
    FILE *fp = fopen("rt_matrix_log.txt", "w");
    if (fp == NULL) {
        perror("fopen");
        // If file can't be opened, fallback to printing to stdout
        printf("Failed to open rt_matrix_log.txt for writing. Printing to stdout instead.\n");
        printf("Logged %d latency samples (ns):\n", latency_index);
        for (int i = 0; i < latency_index; i++)
            printf("%lld\n", latencies[i]);
    } else {
        for (int i = 0; i < latency_index; i++) {
            if (fprintf(fp, "%lld\n", latencies[i]) < 0) {
                perror("fprintf");
                break;
            }
        }
        fclose(fp);
        printf("Logged %d samples to rt_matrix_log.txt\n", latency_index);
    }

    /* ---------- Clean-up ---------- */
    munlockall();

    return EXIT_SUCCESS;
}