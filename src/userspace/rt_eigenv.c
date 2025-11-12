#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <sched.h>
#include <stdio.h>
#include <math.h>

#define PERIOD_NS 10000000L  // 10 ms
#define SIZE 32
#define MAX_ITERATIONS 32
#define LOG_SIZE 10000       // number of timing samples
#define STACK_PREFLT (4 * 1024 * 1024)  // 8 MB

static volatile sig_atomic_t running = 1;
static long long latencies[LOG_SIZE];
static int latency_index = 0;

// allocate matrices statically to prevent stack overflow
float A[SIZE][SIZE];
float eigenvalue;
float next_eigenvector[SIZE];
float eigenvector[SIZE] = {0.0f};



void prefault_stack(void) {
    volatile char stack[STACK_PREFLT];
    for (size_t i = 0; i < STACK_PREFLT; i += 4096)
        stack[i] = 0;
}

static void init_matrix(float matrix[SIZE][SIZE]) {
    for (int i = 0; i < SIZE; ++i) {
        for (int j = 0; j < SIZE; ++j) {
            if (i == j)                     matrix[i][j] = 2.0f;
            else if (j == i - 1 || j == i + 1) matrix[i][j] = -1.0f;
            else                              matrix[i][j] = 0.0f;
        }
    }
}


static float arrmax(float v[SIZE]) {
    float largest = 0.0f;
    for (int i = 0; i < SIZE; i++) {
        float a = fabsf(v[i]);          // abs, not raw value
        if (a > largest) largest = a;
    }
    return largest;
}

static void arrcpy(float src[SIZE], float dest[SIZE]) {
    for (int i = 0; i < SIZE; i++) {
        dest[i] = src[i];
    }
}


void normalize(float v[SIZE]) {
    float max = arrmax(v);
    if (max == 0.0f) return;            // guard first
    for (int i = 0; i < SIZE; i++) v[i] /= max;
}

float dot_product(float v1[SIZE], float v2[SIZE]) {
    float result = 0.0f;
    for (int i = 0; i < SIZE; i++) {
        result += v1[i] * v2[i];
    }
    return result;
}

void matvec_mul(float A[SIZE][SIZE], float v[SIZE], float result[SIZE]) {
    for (int i = 0; i < SIZE; i++) {
        float sum = 0.0f;
        for (int j = 0; j < SIZE; j++) {
            sum += A[i][j] * v[j];
        }
        result[i] = sum;
    }
}

void power_iteration(float A[SIZE][SIZE]) {
    for (int ITER = 0; ITER < MAX_ITERATIONS; ITER++) {
        matvec_mul(A, eigenvector, next_eigenvector);
        float denom = dot_product(eigenvector, eigenvector);
        if (denom < 1e-12f) denom = 1e-12f;
        eigenvalue = dot_product(eigenvector, next_eigenvector) / denom;
        normalize(next_eigenvector);
        arrcpy(next_eigenvector, eigenvector);
    }
}

/* ---------- Signal handling ---------- */
void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

/* ---------- main ---------- */
int main(void) {

    // Quits real-time program loop when SIGINT or SIGTERM is given.
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    
    init_matrix(A);      // Initialize Toeplitz matrix
    eigenvector[0] = 1.0f;  // Setup initial guess as {1, 0, 0, ... , 0}
    /* Setup realtime environment */
    struct sched_param p;
    p.sched_priority = 80;


    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("mlockall");
        return EXIT_FAILURE;
    }
    prefault_stack();

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
        power_iteration(A);
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
    FILE *fp = fopen("rt_eigenv_log.txt", "w");
    if (fp == NULL) {
        perror("fopen");
        // If file can't be opened, fallback to printing to stdout
        printf("Failed to open rt_eigenv_log.txt for writing. Printing to stdout instead.\n");
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
        printf("Logged %d samples to rt_eigenv_log.txt\n", latency_index);
    }

    // ---------- Write eigenvalue and eigenvector to file after loop
    FILE *efp = fopen("rt_eigenv_results.txt", "w");
    if (efp == NULL) {
        printf("Failed to open rt_eigen_results.txt for writing.\n");
    } else {
        fprintf(efp, "%.6f\n", eigenvalue);
        for (int i = 0; i < SIZE; i++) {
            fprintf(efp, "%.6f\n", eigenvector[i]);
        }
        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                fprintf(efp, "%.6f ", A[i][j]);
            }
            fprintf(efp, "\n");
        }

        fclose(efp);
        printf("Eigenvalue, eigenvector, and matrix written to rt_eigenv_results.txt\n");
    }

    /* ---------- Clean-up ---------- */
    munlockall();

    return EXIT_SUCCESS;
}