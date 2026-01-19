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
#include <math.h>
#include <stdatomic.h>
#include <arpa/inet.h>
#include <errno.h>

/* --- DEFINES --- */
#define PERIOD_NS 10000000L     // 10 ms control loop
#define STACK_PREFLT (4*1024*1024) 
#define PI 3.14159265358979323846
#define FFT_SIZE 64 
#define RB_CAPACITY 16 

// Priorities
#define PRIO_FFT     90
#define PRIO_SOCKET  40

static volatile sig_atomic_t running = 1;

/* --- STRUCTS --- */
typedef struct {
    float real;
    float imag;
} complex_t;

typedef struct {
    int core_id;
    // No logging pointer needed anymore!
} thread_args_t;

typedef struct {
    complex_t frames[RB_CAPACITY][FFT_SIZE];
    atomic_size_t head;
    atomic_size_t tail;
} frame_ring_buffer_t;

static frame_ring_buffer_t rb_out;
static complex_t twiddle_table[FFT_SIZE / 2];

/* --- HELPERS --- */
static inline complex_t cmul(complex_t a, complex_t b) {
    return (complex_t){
        a.real * b.real - a.imag * b.imag,
        a.real * b.imag + a.imag * b.real
    };
}
static inline complex_t cadd(complex_t a, complex_t b) {
    return (complex_t){a.real + b.real, a.imag + b.imag};
}
static inline complex_t csub(complex_t a, complex_t b) {
    return (complex_t){a.real - b.real, a.imag - b.imag};
}

int rb_push_frame(complex_t *result_data) {
    size_t head = atomic_load_explicit(&rb_out.head, memory_order_relaxed);
    size_t next_head = (head + 1) & (RB_CAPACITY - 1);
    size_t tail = atomic_load_explicit(&rb_out.tail, memory_order_acquire);
    if (next_head == tail) return 0; // Drop frame
    memcpy(rb_out.frames[head], result_data, sizeof(complex_t) * FFT_SIZE);
    atomic_store_explicit(&rb_out.head, next_head, memory_order_release);
    return 1;
}

int rb_pop_frame(complex_t *out_buffer) {
    size_t tail = atomic_load_explicit(&rb_out.tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&rb_out.head, memory_order_acquire);
    if (tail == head) return 0;
    memcpy(out_buffer, rb_out.frames[tail], sizeof(complex_t) * FFT_SIZE);
    size_t next_tail = (tail + 1) & (RB_CAPACITY - 1);
    atomic_store_explicit(&rb_out.tail, next_tail, memory_order_release);
    return 1;
}

void handle_sigint(int sig) { (void)sig; running = 0; }
void prefault_stack(void) {
    volatile char stack[STACK_PREFLT];
    memset((void*)stack, 0, STACK_PREFLT);
}
void pin_to_core(int core_id) {
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

/* --- FFT LOGIC --- */
void init_twiddle_factors(void) {
    for (int i = 0; i < FFT_SIZE / 2; i++) {
        float angle = -2.0f * PI * i / FFT_SIZE;
        twiddle_table[i].real = cosf(angle);
        twiddle_table[i].imag = sinf(angle);
    }
}

void bit_reverse_permutation(complex_t x[], int N) {
    int j = 0;
    for (int i = 0; i < N; i++) {
        if (i < j) {
            complex_t temp = x[i]; x[i] = x[j]; x[j] = temp;
        }
        int m = N / 2;
        while (m >= 1 && j >= m) { j -= m; m /= 2; }
        j += m;
    }
}

void fft_iterative(complex_t x[], int N) {
    bit_reverse_permutation(x, N);
    for (int len = 2; len <= N; len <<= 1) {
        int half_len = len / 2;
        int twiddle_step = FFT_SIZE / len; 
        for (int i = 0; i < N; i += len) {
            for (int k = 0; k < half_len; k++) {
                complex_t W = twiddle_table[k * twiddle_step];
                complex_t u = x[i + k];
                complex_t v = cmul(W, x[i + k + half_len]);
                x[i + k] = cadd(u, v);
                x[i + k + half_len] = csub(u, v);
            }
        }
    }
}

/* --- THREADS --- */
void* fft_rt_thread(void* arg) {
    thread_args_t* t_args = (thread_args_t*)arg;
    pin_to_core(t_args->core_id);
    prefault_stack();

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    init_twiddle_factors();
    complex_t data_buffer[FFT_SIZE];

    printf("FFT Thread: Running (Core %d)\n", t_args->core_id);

    while (running) {
        // 1. Generate Input (Synthetic)
        for(int i=0; i<FFT_SIZE; i++) {
            float t = (float)i / FFT_SIZE;
            data_buffer[i].real = sinf(2 * PI * 5.0f * t);
            data_buffer[i].imag = 0.0f;
        }

        // 2. Process
        fft_iterative(data_buffer, FFT_SIZE);

        // 3. Push to Socket Thread
        rb_push_frame(data_buffer);

        // 4. Wait for next period
        next.tv_nsec += PERIOD_NS;
        while (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }
    return NULL;
}

void* socket_thread(void* arg) {
    thread_args_t* t_args = (thread_args_t*)arg;
    pin_to_core(t_args->core_id);
    prefault_stack();

    complex_t tx_buffer[FFT_SIZE];
    int sockfd = -1;
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    while (running) {
        if (sockfd < 0) {
            if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) >= 0) {
                if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
                    close(sockfd); sockfd = -1; sleep(1);
                }
            } else sleep(1);
        }

        if (rb_pop_frame(tx_buffer)) {
            if (sockfd >= 0) {
                if (send(sockfd, tx_buffer, sizeof(tx_buffer), MSG_NOSIGNAL) < 0) {
                    close(sockfd); sockfd = -1;
                }
            }
        } else {
            usleep(1000); 
        }
    }
    if (sockfd >= 0) close(sockfd);
    return NULL;
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("mlockall failed"); return EXIT_FAILURE;
    }

    pthread_t fft_tid, socket_tid;
    pthread_attr_t attr;
    struct sched_param param;

    thread_args_t fft_args = { .core_id = 2 };
    thread_args_t sock_args = { .core_id = 3 };

    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

    param.sched_priority = PRIO_FFT;
    pthread_attr_setschedparam(&attr, &param);
    pthread_create(&fft_tid, &attr, fft_rt_thread, &fft_args);

    param.sched_priority = PRIO_SOCKET;
    pthread_attr_setschedparam(&attr, &param);
    pthread_create(&socket_tid, &attr, socket_thread, &sock_args);

    pthread_attr_destroy(&attr);
    pthread_join(fft_tid, NULL);
    pthread_join(socket_tid, NULL);

    munlockall();
    return EXIT_SUCCESS;
}