#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mman.h>
#include <time.h>

#define PERIOD_NS 10000000L   // 10 ms
#define STACK_PREFLT (4 * 1024 * 1024)  // 8 MB

void prefault_stack(void) {
    volatile char stack[STACK_PREFLT];
    for (size_t i = 0; i < STACK_PREFLT; i += 4096)
        stack[i] = 0;
}

int main(void) {
    struct sched_param p = { .sched_priority = 80 };
    mlockall(MCL_CURRENT | MCL_FUTURE);                // 1️⃣ lock memory
    sched_setscheduler(0, SCHED_FIFO, &p);             // 2️⃣ set RT priority

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    while (1) {
        /* --- your work --- */
        printf("tick\n");

        /* --- periodic timing --- */
        next.tv_nsec += PERIOD_NS;
        while (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }
}