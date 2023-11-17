#ifndef UTILS_H
#define UTILS_H

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#define MAXTHREADS 1000
unsigned int ABORT_LIM_MAX = UINT_MAX;

// Thread stuff
int worker_id[MAXTHREADS];
pthread_t worker_threads[MAXTHREADS];
static sem_t thread_lock_start[MAXTHREADS];
static sem_t thread_lock_end;
char thread_names[MAXTHREADS][16];
// counters for threads, how often they ran
uint64_t thread_runs[MAXTHREADS] = {0};
uint64_t enc_unlocked[MAXTHREADS] = {0};

// Barrier stuff
pthread_barrier_t start_barrier; 
pthread_barrier_t stop_barrier;


struct threadParam {
    int id;
    unsigned int sleep;
} threadParams[MAXTHREADS];

const char* sched_policy[] = {"SCHED_OTHER", "SCHED_FIFO", "SCHED_RR", "SCHED_BATCH"};
const char* lockTypeStr[] = {"SEMAPHORES", "BARRIERS"};

struct arguments {
    unsigned int numthreads;
    unsigned int iterations;
    unsigned int cpumask;
    unsigned int abortlimit;
    unsigned int forceexit;
    unsigned int nsleepthread;
    unsigned int nsleepmain;
    unsigned int schedthread;
    unsigned int schedmain;
    unsigned int locktype
};
struct arguments args;

static double calculateSD(double data[], unsigned int length) {
    double sum = 0.0, mean, standardDeviation = 0.0;
    int i;
    for (i = 0; i < length; ++i) {
        sum += data[i];
    }
    mean = sum / length;
    for (i = 0; i < length; ++i) {
        standardDeviation += pow(data[i] - mean, 2);
    }
    return sqrt(standardDeviation / length);
}

static void print_help() {
    printf("PLease provide the following parameteres:\n"
           "<numthreads>   \t\t number of worker threads\n"
           "<iterations>   \t\t number of iterations \n"
           "<cpumask>      \t\t cpu mask, to place threads \n"
           "<abortlimit>   \t\t Abort in us if measurement exceeds this value \n"
           "<forceexit>    \t\t Exit(1) without printing summary \n"
           "<nanosleepthread>\t Sleep in Thread during in work part \n"
           "<nanosleepmain>\t\t Sleep in main after iteration \n"
           "<schedthread>    \t Scheduler of worker threads --> 0=OTHER, 1=FIFO, 2=RR\n"
           "<schedmain>      \t Scheduler of main thread --> 0=OTHER, 1=FIFO, 2=RR \n"
           "<locktype>     \t\t Type of test, 0=Semaphores, 1=Barriers\n"
           );
}

static void display_summary(struct arguments* args) {
    printf("\n-----------------------------\n");
    printf("Summary before start:\n");
    printf("<numthreads>   \t\t %u\n"
           "<iterations>   \t\t %u\n"
           "<cpumask>      \t\t %u\n"
           "<abortlimit>   \t\t %uus\n"
           "<forceexit>    \t\t %u\n"
           "<nanosleepthread>\t %uns\n"
           "<nanosleepmain>\t\t %uns\n"
           "<schedthread>    \t %s\n"
           "<schedmain>      \t %s\n"
           "<locktype>       \t %s\n",
           args->numthreads, args->iterations, args->cpumask, args->abortlimit, args->forceexit,
           args->nsleepthread, args->nsleepmain, sched_policy[args->schedthread],
           sched_policy[args->schedmain], lockTypeStr[locktype]);
    printf("\n-----------------------------\n");
}

void* thread_func(void* context);

static int init_threads(struct arguments* args) {
    int ret;

    int thread_priority;
    struct sched_param sched_params;
    memset(&sched_params, 0, sizeof(sched_params));

    thread_priority = 15 + 60;

    if (args.locktype == 0) {
        ret = sem_init(&thread_lock_end, 0, 0);
        if (ret != 0) {
            printf("Error init sem %d %s", ret, strerror(errno));
            return ret;
        }
    } else if (args.locktype == 1) {
        // all threads + main thread need to enter the barriers, to continue
        ret = pthread_barrier_init (&start_barrier, NULL, args.numthreads+1); 
        ret = pthread_barrier_init (&stop_barrier, NULL, args.numthreads+1);  
    }

    for (int i = 0; i < args->numthreads; i++) {
        if (args.locktype == 0) {
            ret = sem_init(&thread_lock_start[i], 0, 0);
            if (ret != 0) {
                printf("Error init sem %d %s", ret, strerror(errno));
                return ret;
            }
        }
        // create thread
        // worker_id[i] = i;
        threadParams[i].id = i;//worker_id[i];
        threadParams[i].sleep = args->nsleepthread;

        if (args->locktype == 0) { // sem_pend + sem_post
            ret = pthread_create(&worker_threads[i], NULL, thread_func_semaphores, &threadParams[i]);
            if (ret != 0) {
                printf("create thread %d errno %s\n", ret, strerror(errno));
                return ret;
            }
        } else if (args.locktype == 1) { // barrier_wait + barrier_enter
            ret = pthread_create(&worker_threads[i], NULL, thread_func_barriers, &threadParams[i]);
            if (ret != 0) {
                printf("create thread %d errno %s\n", ret, strerror(errno));
                return ret;
            }
        }

        snprintf(thread_names[i], 32, "workerthread%u", (unsigned int)i);
        ret = pthread_setname_np(worker_threads[i], (const char*)thread_names[i]);
        if (ret != 0) {
            printf("pthread_setname_np %d errno %s\n", ret, strerror(errno));
            return ret;
        }

        // set CPU affinity
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);

        int core = 0;
        if (args->cpumask == 0x1) {
            core = 0; // core 0
        } else if (args->cpumask == 0x2) {
            core = 1; // core 1
        } else if (args->cpumask == 0x3) {
            core = (i % 2); // core 01
        } else if (args->cpumask == 0x4) {
            core = 3; // core 2
        } else if (args->cpumask == 0x5) {
            core = 0 ? (i % 2 == 0) : 2; // core 02
        } else if (args->cpumask == 0x6) {
            core = 1 ? (i % 2 == 0) : 2; // core 12
        } else if (args->cpumask == 0x7) {
            core = i % 3; // core 012
        } else if (args->cpumask == 0x8) {
            core = 3; // core 3
        } else if (args->cpumask == 0x9) {
            core = 0 ? (i % 2 == 0) : 3; // core 03
        } else if (args->cpumask == 0xA) {
            core = 1 ? (i % 2 == 0) : 3; // core 13
        } else if (args->cpumask == 0xB) {
            core = (i % 3); // core 013
            if (core == 2)
                core = 3;
        } else if (args->cpumask == 0xC) {
            core = 2 ? (i % 2 == 0) : 3; // core 23
        } else if (args->cpumask == 0xD) {
            core = (i % 3); // core 023
            if (core == 2)
                core = 3;
            else if (core == 1)
                core = 2;
        } else if (args->cpumask == 0xE) {
            core = (i % 3) + 1; // core 123
        } else if (args->cpumask == 0xF) {
            core = (i % 4); // core 0123
        }

        if (i < 5) {
            printf("Placing Thread%u on Core%d\n", i, core);
        } else if (i == 5) {
            printf("....");
        }
        CPU_SET(core, &cpuset);
        ret = pthread_setaffinity_np(worker_threads[i], sizeof(cpuset), &cpuset);
        if (ret != 0) {
            printf("pthread_setaffinity_np thread %u errno %s\n", ret, strerror(errno));
            // return ret;
        }

        // set priorities
        sched_params.sched_priority = thread_priority;
        ret = pthread_setschedparam(worker_threads[i], args->schedthread, &sched_params);
        if (ret != 0) {
            printf("pthread_setschedparam with prio %u and Scheduler %s, thread %u --> errno "
                   "%s\n",
                   thread_priority, sched_policy[args->schedthread], i, strerror(errno));
            // return ret;
        }
    }
    return 0;
}

#endif // UTILS_H