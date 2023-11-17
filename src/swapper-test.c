/*
requires: -D__linux__
*/
#define _GNU_SOURCE

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

void* thread_func_barriers(void* context) {
    struct threadParam *args = context;

    int thread_id = args->id;
    unsigned int sleep = args->sleep;

    int ret;
    for (;;) {
        ret = pthread_barrier_wait(&start_barrier);
        if (ret < 0) {
            printf("ERROR start_barrier wait %d (thread %u)\n", ret, thread_id);
        }
        thread_runs[thread_id]++;
        if (nanosleep > 0) {
            nanosleep((const struct timespec[]){{0, sleep}}, NULL);
        }
        ret = pthread_barrier_wait(&stop_barrier);
        if (ret < 0) {
            printf("ERROR stop_barrier wait %d (thread %u)\n", ret, thread_id);
        }
    }
}


void* thread_func_semaphores(void* context) {
    struct threadParam *args = context;

    int thread_id = args->id;
    unsigned int sleep = args->sleep;

    int ret;
    for (;;) {
        ret = sem_wait(&thread_lock_start[thread_id]);
        if (ret < 0) {
            printf("ERROR sem_wait %d (thread %u)\n", ret, thread_id);
        }
        thread_runs[thread_id]++;
        if (nanosleep > 0) {
            nanosleep((const struct timespec[]){{0, sleep}}, NULL);
        }
        ret = sem_post(&thread_lock_end);
        if (ret == 0) {
            enc_unlocked[thread_id]++;
        } else {
            printf("%d Error sem_post %u (%s)\n", thread_id, ret, strerror(errno));
        }
    }
}

int main(int argc, char** argv) {
    args.numthreads = 1;        // 1 worker thread
    args.iterations = 1000;     // 1000 iterations
    args.cpumask = 1;           // running on Core0
    args.abortlimit = UINT_MAX; // UINT_MAX us abort limit
    args.forceexit = 0;         // print results in case of abortlimit is breached
    args.nsleepthread = 0;      // no sleep in threads
    args.nsleepmain = 0;        // no sleep after iteration in main
    args.schedthread = 2;       // RR scheduler
    args.schedmain = 2;         // RR scheduler
    args.locktype = 0;          // Semaphore Test

    if (argc < 2) {
        printf("Missing parameters! \n");
        print_help();
        exit(0);
    } else if (argc >= 2) {
        args.numthreads = atoi(argv[1]);
        args.iterations = atoi(argv[2]);
        args.cpumask = atoi(argv[3]);
        if (argc >= 3) {
            args.abortlimit = atoi(argv[4]);
            if (args.abortlimit < 0) {
                printf("Not aborting, set valid abort limit > 0");
                args.abortlimit = 0;
            }
        }
        if (argc >= 4) {
            args.forceexit = atoi(argv[5]);
            if (args.forceexit != 0) {
                args.forceexit = 1; // 0 means disabled, any other value means enabled
            }
        }
        if (argc >= 5) {
            args.nsleepthread = atoi(argv[6]);
            if (args.nsleepthread < 0) {
                {
                    args.nsleepthread = 0;
                    printf("Using default: Thread sleeps for %uns\n", args.nsleepthread);
                }
            }
            if (argc >= 6) {
                args.nsleepmain = atoi(argv[7]);
                if (args.nsleepmain < 0) {
                    args.nsleepmain = 0;
                    printf("Using default: Main sleeps for %uns\n", args.nsleepmain);
                }
            }
            if (argc >= 7) {
                int schedthread = atoi(argv[8]);
                if (schedthread == 0) {
                    args.schedthread = SCHED_OTHER;
                } else if (schedthread == 1) {
                    args.schedthread = SCHED_FIFO;
                } else if (schedthread == 2) {
                    args.schedthread = SCHED_RR;
                } else {
                    printf("Invalid Sched policy, using SCHED_RR\n");
                }
            }

            if (argc >= 8) {
                int schedmain = atoi(argv[9]);
                if (schedmain == 0) {
                    args.schedmain = SCHED_OTHER;
                } else if (schedmain == 1) {
                    args.schedmain = SCHED_FIFO;
                } else if (schedmain == 2) {
                    args.schedmain = SCHED_RR;
                } else {
                    printf("Invalid Sched policy, using SCHED_RR\n");
                }
            }

            if (args.numthreads > MAXTHREADS) {
                printf("Max 1000 Threads supported! \n");
                return -1;
            }
            if (args.iterations <= 0) {
                printf("Only positive amount of iterations ;) \n");
                return -1;
            }
            if (args.cpumask > 0xF || args.cpumask < 0x1) {
                printf("Mask 0xF is maximum! (4 cores)\n");
                printf("EXAMPLES\n");
                printf("  1  = 0b0001-> CORE 0 only\n");
                printf("  3  = 0b0011-> CORES 0 & 1\n");
                printf("  14 = 0b1110-> CORES 1 & 2 & 3\n");
                return -1;
            }
            if (args.abortlimit < 0) {
                printf("Abort limit > 0 in us\n");
                return -1;
            }
        }

        struct sched_param sp;
        sp.sched_priority = 50;
        if (args.schedmain == SCHED_OTHER) {
            sp.sched_priority = 0;
        }

        pid_t pid = getpid();
        int ret = sched_setscheduler(pid, args.schedmain, &sp);
        if (ret != 0) {
            printf("Return value %d = %s: Setting main Scheduler not possible!\n", ret, strerror(errno));
        }
        args.schedmain = sched_getscheduler(pid);
        printf("Main Scheduler is: %s.\n", sched_policy[args.schedmain]);

        display_summary(&args);

        init_threads(&args); // todo add prio

        // usleep(50*1000);
        if (args.abortlimit > 0) {
            printf("Aborting if time >%ums is measured\n", args.abortlimit);
        }

        double runtime[args.iterations];

        unsigned int numIterated = 0;
        double max = 0;
        double min = UINT64_MAX;
        double mean = 0;

        struct timespec time_start, time_finish;
        double elapsed;

        while (numIterated < args.iterations) {
            clock_gettime(CLOCK_MONOTONIC, &time_start);
            if (args.locktype == 0) { // semaphore testing
                for (int i = 0; i < args.numthreads; i++) {
                    ret = sem_post(&thread_lock_start[i]);
                }
                for (int i = 0; i < args.numthreads; i++) {
                    ret = sem_wait(&thread_lock_end);
                }
            } else if (args.locktype == 1) { // barrier testing
                ret = pthread_barrier_wait(&start_barrier);
                if (ret < 0) {
                    printf("ERROR start_barrier wait %d (main)\n", ret);
                }
                ret = pthread_barrier_wait(&stop_barrier);
                if (ret < 0) {
                    printf("ERROR stop_barrier wait %d (main)\n", ret);
                }
            }

            clock_gettime(CLOCK_MONOTONIC, &time_finish);
            elapsed = (time_finish.tv_sec - time_start.tv_sec) * 1000000;
            elapsed += (time_finish.tv_nsec - time_start.tv_nsec) / 1000;
            if (elapsed > max) {
                max = elapsed;
            } else if (elapsed < min) {
                min = elapsed;
            }
            mean += elapsed;
            runtime[numIterated] = elapsed;
            numIterated++;
            if (max > args.abortlimit) {
                if (args.forceexit == 1) {
                    exit(1);
                }
                printf("ABORT: %s-Iteration needed: %03fuss in iteration %u\n", lockTypeStr[args.locktype], max, numIterated);
                printf("ABORT: boundary = %u\n", args.abortlimit);
                break;
            }
            if (args.nsleepmain > 0) {
                nanosleep((const struct timespec[]){{0, args.nsleepmain}}, NULL);
            }
        }

        double variance = calculateSD(&runtime[0], numIterated);
        mean = mean / numIterated;

        printf("################## SUMMARY ########################\n");
        printf("Type of test: %s", lockTypeStr[args.locktype]);
        printf("Threads: %u \t\t Iterations: %u/%u\n", args.numthreads, numIterated,
               args.iterations);
        printf("Min: %02fus, \t Max: %02fus\n", min, max);
        printf("Mean: %02fus \t Variance: %02fus \n", mean, variance);
        printf("###################################################\n");

        for (int i = 0; i < args.numthreads; i++) {
            if (thread_runs[i] != numIterated) {
                printf("!! ERROR Thread%u ran %ld/%u\n", i, thread_runs[i], numIterated);
            }
            if (enc_unlocked[i] != numIterated) {
                printf("!! ERROR Thread%u unlocked %ld/%u\n", i, thread_runs[i], numIterated);
            }
        }
    }
}