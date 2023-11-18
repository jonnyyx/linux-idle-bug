/*
requires: -D__linux__
 */
#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXTHREADS 1000
unsigned int ABORT_LIM_MAX = UINT_MAX;

// Thread stuff
int worker_id[MAXTHREADS];
pthread_t worker_threads[MAXTHREADS];
char thread_names[MAXTHREADS][16];

// Barrier stuff
pthread_barrier_t start_barrier;
pthread_barrier_t stop_barrier;

struct threadParam {
    int id;
    unsigned int sleep;
} threadParams[MAXTHREADS];

struct arguments {
    unsigned int numthreads;
    unsigned int iterations;
    unsigned int abortlimit;
    unsigned int nsleepthread;
    unsigned int nsleepmain;
};
struct arguments args;

static void* thread_func_barriers(void* context) {
    struct threadParam* args = context;

    int thread_id = args->id;
    unsigned int sleep = args->sleep;

    int ret;
    for (;;) {
        ret = pthread_barrier_wait(&start_barrier);
        if (nanosleep > 0) {
            nanosleep((const struct timespec[]){{0, sleep}}, NULL);
        }
        ret = pthread_barrier_wait(&stop_barrier);
    }
}

static int init_threads(struct arguments* args) {
    int ret;
    // all threads + main thread need to enter the barriers, to continue
    ret = pthread_barrier_init(&start_barrier, NULL, args->numthreads + 1);
    ret = pthread_barrier_init(&stop_barrier, NULL, args->numthreads + 1);

    for (int i = 0; i < args->numthreads; i++) {
        // create thread
        threadParams[i].id = i;
        threadParams[i].sleep = args->nsleepthread;

        ret = pthread_create(&worker_threads[i], NULL, thread_func_barriers, &threadParams[i]);
        if (ret != 0) {
            printf("create thread %d errno %s\n", ret, strerror(errno));
            return ret;
        }

        snprintf(thread_names[i], 32, "workerthread%u", (unsigned int)i);
        ret = pthread_setname_np(worker_threads[i], (const char*)thread_names[i]);
        if (ret != 0) {
            printf("pthread_setname_np %d errno %s\n", ret, strerror(errno));
            return ret;
        }
    }
    return 0;
}

static void print_help() {
    printf("PLease provide the following parameteres:\n"
           "<numthreads>   \t\t number of worker threads\n"
           "<iterations>   \t\t number of iterations \n"
           "<abortlimit>   \t\t Abort in us if measurement exceeds this value \n"
           "<nanosleepthread>\t Sleep in Thread during in work part \n"
           "<nanosleepmain>\t\t Sleep in main after iteration \n");
}

static void display_summary(struct arguments* args) {
    printf("\n-----------------------------\n");
    printf("Summary before start:\n");
    printf("<numthreads>   \t\t %u\n"
           "<iterations>   \t\t %u\n"
           "<abortlimit>   \t\t %uus\n"
           "<nanosleepthread>\t %uns\n"
           "<nanosleepmain>\t\t %uns\n",
           args->numthreads, args->iterations, args->abortlimit, args->nsleepthread,
           args->nsleepmain);
    printf("-----------------------------\n");
}

int main(int argc, char** argv) {
    args.numthreads = 1;        // 1 worker thread
    args.iterations = 10000;    // 1000 iterations
    args.abortlimit = UINT_MAX; // UINT_MAX us abort limit
    args.nsleepthread = 0;      // no sleep in threads
    args.nsleepmain = 0;        // no sleep after iteration in main
    int ret;

    if (argc < 5) {
        printf("Missing parameters! \n");
        print_help();
        exit(0);
    }
    args.numthreads = atoi(argv[1]);
    args.iterations = atoi(argv[2]);
    args.abortlimit = atoi(argv[3]);
    if (args.abortlimit < 0) {
        printf("Not setting abortlimit! Please set >0\n");
        args.abortlimit = 0;
    }
    args.nsleepthread = atoi(argv[4]);
    if (args.nsleepthread < 0) {
        {
            args.nsleepthread = 0;
            printf("Using default: No sleep in worker threads\n");
        }
    }
    args.nsleepmain = atoi(argv[5]);
    if (args.nsleepmain < 0) {
        args.nsleepmain = 0;
        printf("Using default: No sleep in main thread\n");
    }

    char me[] = "main_thread";
    ret = pthread_setname_np(pthread_self(), me);
    if (ret != 0) {
        printf("pthread_setname_np %d errno %s\n", ret, strerror(errno));
        return ret;
    }

    display_summary(&args);

    init_threads(&args);

    if (args.abortlimit > 0) {
        printf("Aborting if > %.3fms is measured\n", args.abortlimit / 1000.0);
    }

    unsigned int numIterated = 0;
    double max = 0;
    double min = UINT64_MAX;

    struct timespec time_start, time_finish;
    double elapsed;

    while (numIterated < args.iterations) {
        clock_gettime(CLOCK_MONOTONIC, &time_start);
        ret = pthread_barrier_wait(&start_barrier);
        ret = pthread_barrier_wait(&stop_barrier);

        clock_gettime(CLOCK_MONOTONIC, &time_finish);
        elapsed = (time_finish.tv_sec - time_start.tv_sec) * 1000000;
        elapsed += (time_finish.tv_nsec - time_start.tv_nsec) / 1000;
        numIterated++;
        if (elapsed > args.abortlimit) {
            printf("ABORT: Iteration %u needed: %.3fms\n", numIterated, max / 1000.0);
            exit(1);
        }
        if (args.nsleepmain > 0) {
            nanosleep((const struct timespec[]){{0, args.nsleepmain}}, NULL);
        }
    }
    printf("\nNO ERROR DETECTED\n");
}
