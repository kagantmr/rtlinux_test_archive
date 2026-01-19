#define _GNU_SOURCE 
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <sched.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

// Configuration
#define PERIOD_NS 10000000L     // 10 ms control loop
#define LOG_SIZE 10000          // Log buffer size
#define STACK_PREFLT (4*1024*1024) // 4MB Stack

// Thread Priorities (SCHED_FIFO ranges 1-99)
#define PRIO     80
#define PRIO_SOCKET  40

static volatile sig_atomic_t running = 1;

// Thread Arguments Structure
typedef struct {
    int core_id;
    long long *latency_log; // Pointer to thread-local log
} thread_args_t;

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

void prefault_stack(void) {
    volatile char stack[STACK_PREFLT];
    memset((void*)stack, 0, STACK_PREFLT);
}

// Helper to set affinity to a specific core
void pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        perror("pthread_setaffinity_np");
    }
}

// --- THREAD 1: HIGH PRIORITY FFT ---
void* fft_rt_thread(void* arg) {
    thread_args_t* t_args = (thread_args_t*)arg;
    pin_to_core(t_args->core_id);
    prefault_stack();

    struct timespec start, end, next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    int log_idx = 0;

    while (running) {
        // 1. Mark start time
        clock_gettime(CLOCK_MONOTONIC, &start);

        // 2. DO THE WORK (FFT Calculation Here)
        // do_fft_calculation();

        // 3. Mark end time & Log
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        long long elapsed = (end.tv_sec - start.tv_sec) * 1000000000LL +
                            (end.tv_nsec - start.tv_nsec);
        
        // Log locally to avoid race conditions
        if (log_idx < LOG_SIZE && t_args->latency_log) {
            t_args->latency_log[log_idx++] = elapsed;
        }

        // 4. Wait for next period
        next.tv_nsec += PERIOD_NS;
        while (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }
    
    printf("FFT Thread: Finished.\n");
    return NULL;
}

// --- THREAD 2: LOWER PRIORITY SOCKET ---
void* socket_thread(void* arg) {
    thread_args_t* t_args = (thread_args_t*)arg;
    pin_to_core(t_args->core_id);
    prefault_stack();

    // This thread might block on I/O, so it must have lower priority
    // to avoid starving the FFT thread.
    while (running) {
        // Check for data from FFT thread (Ring Buffer consumer)
        // Send to network...
        
        // Sleep briefly to prevent 100% CPU usage if no I/O blocks
        usleep(50000); 
    }
    
    printf("Socket Thread: Finished.\n");
    return NULL;
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    // 1. Lock Memory (Crucial for RT)
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("mlockall failed");
        return EXIT_FAILURE;
    }

    pthread_t fft_tid, socket_tid;
    pthread_attr_t attr;
    struct sched_param param;

    // Allocate memory for logs outside the RT loop
    long long* fft_log = malloc(sizeof(long long) * LOG_SIZE);

    // Prepare Arguments
    thread_args_t fft_args = { .core_id = 0, .latency_log = fft_log };
    thread_args_t sock_args = { .core_id = 1, .latency_log = NULL };

    // Initialize Attributes
    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

    // --- CREATE FFT THREAD (PRIORITY 80) ---
    param.sched_priority = PRIO;
    pthread_attr_setschedparam(&attr, &param);
    if (pthread_create(&fft_tid, &attr, fft_rt_thread, &fft_args) != 0) {
        perror("Failed to create FFT thread");
        return EXIT_FAILURE;
    }

    // --- CREATE SOCKET THREAD (PRIORITY 40) ---
    // We reuse the 'attr' object, just update the priority
    param.sched_priority = PRIO_SOCKET;
    pthread_attr_setschedparam(&attr, &param);
    if (pthread_create(&socket_tid, &attr, socket_thread, &sock_args) != 0) {
        perror("Failed to create Socket thread");
        return EXIT_FAILURE;
    }

    pthread_attr_destroy(&attr);

    // 2. BLOCK MAIN (The fix for your "instant exit" bug)
    // We wait here until the threads finish (when running=0)
    pthread_join(fft_tid, NULL);
    pthread_join(socket_tid, NULL);

    // Cleanup
    free(fft_log);
    munlockall();
    printf("Main: Exiting cleanly.\n");
    return EXIT_SUCCESS;
}