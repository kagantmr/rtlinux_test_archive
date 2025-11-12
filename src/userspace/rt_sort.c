#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <sched.h>
#include <stdio.h>

#define PERIOD_NS 10000000L  // 10 ms
#define SIZE 64
#define LOG_SIZE 10000       // number of timing samples

static volatile sig_atomic_t running = 1;
static long long latencies[LOG_SIZE];
static int latency_index = 0;

/* ---------- Signal Handling ---------- */
void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

/* ---------- Merge Sort Implementation ---------- */
static int merge_buffer[SIZE];

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

void merge_sort(int *array, int start, int end) {
    if (start < end) {
        int middle = (start + end) / 2;
        merge_sort(array, start, middle);
        merge_sort(array, middle + 1, end);
        merge(array, start, middle, end);
    }
}

/* ---------- Tiny RNG for test data ---------- */
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

/* ---------- Main ---------- */
int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    int *arr = calloc(SIZE, sizeof(int));
    if (!arr) {
        perror("calloc");
        return EXIT_FAILURE;
    }

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
        fill_array(arr);

        clock_gettime(CLOCK_MONOTONIC, &start);
        merge_sort(arr, 0, SIZE - 1);
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

    /* ---------- Write Logged Timings to File ---------- */
    FILE *fp = fopen("rt_sort_log.txt", "w");
    if (fp == NULL) {
        perror("fopen");
        /* If file can't be opened, fallback to printing to stdout */
        printf("Failed to open rt_sort_log.txt for writing. Printing to stdout instead.\n");
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
        printf("Logged %d samples to rt_sort_log.txt\n", latency_index);
    }

    /* ---------- Cleanup ---------- */
    munlockall();
    free(arr);

    return EXIT_SUCCESS;
}