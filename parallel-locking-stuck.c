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
#include <float.h>

#define MAXTHREADS 1000
unsigned int ABORT_LIM_MAX = UINT_MAX;

int worker_id[MAXTHREADS];
pthread_t worker_threads[MAXTHREADS];
char thread_names[MAXTHREADS][16];

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
} args;

static inline void xpthread_barrier_wait(pthread_barrier_t *barrier)
{
	int ret = pthread_barrier_wait(barrier);
	if (ret == 0 || ret == PTHREAD_BARRIER_SERIAL_THREAD) {
		// fine
		return;
	} else {
		// Some other error occurred
		fprintf(stderr, "Error: pthread_barrier_wait failed with error code %d\n", ret);
		exit(EXIT_FAILURE);
	}
}

static void xpthread_set_selfname(const char *name) {
	int ret;
        ret = pthread_setname_np(pthread_self(), name);
        if (ret != 0) {
            printf("pthread_setname_np %d errno %s\n", ret, strerror(errno));
            exit(EXIT_FAILURE);
        }
}

static void* thread_func_barriers(void* context) {
    struct threadParam* args = context;
    unsigned int sleep = args->sleep;
    int thread_id = args->id;
    char name[16];

    sprintf(name, "workerthread%u", (unsigned int)thread_id);
    xpthread_set_selfname(name);

    while (1) {
        xpthread_barrier_wait(&start_barrier);
        if (sleep > 0) {
            nanosleep((const struct timespec[]){{0, sleep}}, NULL);
        }
        xpthread_barrier_wait(&stop_barrier);
    }
}


static int init_threads(struct arguments* args) {
    int ret;

    // all threads + main thread need to enter the barriers, to continue
    pthread_barrier_init(&start_barrier, NULL, args->numthreads + 1);
    pthread_barrier_init(&stop_barrier, NULL, args->numthreads + 1);

    for (unsigned i = 0; i < args->numthreads; i++) {
        // create thread
        threadParams[i].id = i;
        threadParams[i].sleep = args->nsleepthread;

        ret = pthread_create(&worker_threads[i], NULL, thread_func_barriers, &threadParams[i]);
        if (ret != 0) {
            printf("create thread %d errno %s\n", ret, strerror(errno));
            return ret;
        }

    }

    // we sleep here 10ms, just to make sure all threads had their
    // time to initialze their barriers
    nanosleep((const struct timespec[]){{0, 10000}}, NULL);
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

static void display_summary(struct arguments *args) {
    printf("Used parameter:\n"
	   "  Number if threads:        %u\n"
           "  Scheduled iterations:     %u\n"
           "  Abortlimit:               %uus\n"
           "  Sleeptime in thread:      %uns\n"
           "  Sleeptime in main thread: %uns\n",
           args->numthreads, args->iterations, args->abortlimit, args->nsleepthread,
           args->nsleepmain);
}

int parse_args(int argc, char **argv)
{
    args.numthreads = 1;
    args.iterations = 10000;
    args.abortlimit = UINT_MAX;
    args.nsleepthread = 0;
    args.nsleepmain = 0;

    if (argc < 5) {
        printf("Missing parameters!\n");
	return -EINVAL;
    }
    args.numthreads = atoi(argv[1]);
    if (args.numthreads >= MAXTHREADS) {
	    fprintf(stderr, "Just %u threads allowed, exiting", MAXTHREADS);
	    return -EINVAL;
    }

    args.iterations = atoi(argv[2]);
    args.abortlimit = atoi(argv[3]);
    if (args.abortlimit == 0) {
        printf("Not setting abortlimit! Please set >0\n");
        args.abortlimit = 0;
    }
    args.nsleepthread = atoi(argv[4]);
    if (args.nsleepthread == 0) {
        {
            args.nsleepthread = 0;
            printf("Using default: No sleep in worker threads\n");
        }
    }
    args.nsleepmain = atoi(argv[5]);
    if (args.nsleepmain == 0) {
        printf("Using default: No sleep in main thread\n");
    }

    return 0;
}

int main(int argc, char **argv) {
    int ret;
    unsigned int i = 0;
    char me[] = "main_thread";
    double max = 0;
    double min = DBL_MAX;
    struct timespec time_start, time_finish;
    double elapsed;

    ret = parse_args(argc, argv);
    if (ret < 0) {
        print_help();
	exit(EXIT_FAILURE);
    }
    display_summary(&args);

    init_threads(&args);
    xpthread_set_selfname(me);

    printf("Info: program will exit if loop processing time above %uus\n",
	   args.abortlimit);

    while (i < args.iterations) {
        clock_gettime(CLOCK_MONOTONIC, &time_start);
        xpthread_barrier_wait(&start_barrier);
        xpthread_barrier_wait(&stop_barrier);

        clock_gettime(CLOCK_MONOTONIC, &time_finish);
        elapsed = (time_finish.tv_sec - time_start.tv_sec) * 1000000.0;
        elapsed += (time_finish.tv_nsec - time_start.tv_nsec) / 1000.0;
        i++;
        if (elapsed > args.abortlimit) {
            printf("\033[31mThreshold triggered! Loop processing time %.0lfus above threshold of %uus\033[0m\n",
		   elapsed, args.abortlimit);
	    printf("This happends in iteration %u\n", i);
            exit(1);
        }
        if (args.nsleepmain > 0) {
            nanosleep((const struct timespec[]){{0, args.nsleepmain}}, NULL);
        }
    }
    printf("No threshold violation detected! All loop within range %fus\n", args.abortlimit);
};
