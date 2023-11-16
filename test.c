/*
requires: -D__linux__
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <semaphore.h>
#include <math.h>
#include <limits.h>


#define PARALLEL_ENCODING_CORES_MAX 1000
int PARALLEL_ENCODING_CORES = 0;
int NUM_ITERATIONS = 0;
int CPU_MASK = 0x0;
unsigned int ABORT_LIM = UINT_MAX;
int FEXIT = 0;
int NANOSLEEPTHREAD = 0;
int NANOSLEEPMAIN = 0;

int CODING_SCHEDULER = SCHED_RR;
int CODING_SCHEDULER_MAIN = SCHED_RR;

int worker_id_enc[PARALLEL_ENCODING_CORES_MAX];

pthread_t encoder_worker_threads[PARALLEL_ENCODING_CORES_MAX];

static sem_t encoder_lock_start[PARALLEL_ENCODING_CORES_MAX];
static sem_t encoder_lock_end;

char encoder_names[PARALLEL_ENCODING_CORES_MAX][16];

uint64_t enc_run[PARALLEL_ENCODING_CORES_MAX] = {0};
uint64_t enc_unlocked[PARALLEL_ENCODING_CORES_MAX] = {0};


const char *sched_policy[] = {
    "SCHED_OTHER",
    "SCHED_FIFO",
    "SCHED_RR",
    "SCHED_BATCH"
};

void *encoding_worker_thread_func(void *id_arg) {
  int thread_id = *((int *)id_arg);
  int ret;
  for (;;) {
    ret = sem_wait(&encoder_lock_start[thread_id]);
    if (ret < 0) {
      printf("ERROR sem_wait %d (thread %d)\n", ret, thread_id);
    }
    enc_run[thread_id]++;
    if (NANOSLEEPTHREAD > 0) {
        nanosleep((const struct timespec[]){{0, NANOSLEEPTHREAD}}, NULL);
    }
    ret = sem_post(&encoder_lock_end);
    if (ret == 0) {
      enc_unlocked[thread_id]++;
    } else {
      printf("%d Error sem_post %d (%s)\n", thread_id, ret, strerror(errno));
    }

  }
}


int init_coding() {
  int ret;

    int thread_priority_encoder;
    int thread_priority_decoder;
    struct sched_param params;
    memset(&params, 0, sizeof(params));
    int rc;
    thread_priority_encoder = 15 + 60;

    ret = sem_init(&encoder_lock_end, 0, 0);
    if (ret != 0) {
      printf("ERROR init0 sem %d %s", ret, strerror(errno));
      return ret;
    }

    printf("Creating %d Threads!\n", PARALLEL_ENCODING_CORES);
    for(int i=0; i<PARALLEL_ENCODING_CORES;i++) {

      ret = sem_init(&encoder_lock_start[i], 0, 0);
      if (ret != 0) {
        printf("ERROR init sem %d %s", ret, strerror(errno));
        return ret;
      }
      // create thread
      worker_id_enc[i] = i;
      ret = pthread_create(&encoder_worker_threads[i], NULL,
                           encoding_worker_thread_func, &worker_id_enc[i]);
      if (ret != 0) {
        printf("create enc %d errno %s\n", ret, strerror(errno));
        return ret;
      }

      snprintf(encoder_names[i], 16, "test_thread%i", i);
      ret = pthread_setname_np(encoder_worker_threads[i], (const char *)encoder_names[i]);
      if (ret != 0) {
        printf("pthread_setname_np %d errno %s\n", ret, strerror(errno));
        return ret;
      }

      // set CPU affinity
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);

      int core = 0;
      if (CPU_MASK == 0x1) {
          core = 0;                 //core 0
      } else if (CPU_MASK == 0x2) {
          core = 1;                 //core 1
      } else if (CPU_MASK == 0x3) {
          core = (i % 2);           //core 01
      } else if (CPU_MASK == 0x4) {
          core = 3;                 //core 2
      } else if (CPU_MASK == 0x5) {
          core = 0 ? (i%2==0) : 2;  //core 02
      } else if (CPU_MASK == 0x6) {
          core = 1 ? (i%2==0) : 2;  //core 12
      } else if (CPU_MASK == 0x7) {
          core = i % 3;             //core 012
      } else if (CPU_MASK == 0x8) {
          core = 3;                 //core 3
      } else if (CPU_MASK == 0x9) {
          core = 0 ? (i%2==0) : 3;  //core 03
      } else if (CPU_MASK == 0xA) {
          core = 1 ? (i%2==0) : 3;  //core 13
      } else if (CPU_MASK == 0xB) {
          core = (i % 3);           //core 013
          if (core == 2) core=3;
      } else if (CPU_MASK == 0xC) {
          core = 2 ? (i%2==0) : 3;  //core 23
      } else if (CPU_MASK == 0xD) {
          core = (i % 3);           //core 023
          if (core==2) core=3;
          else if (core == 1) core=2;
      } else if (CPU_MASK == 0xE) {
          core = (i % 3) + 1;       //core 123
      } else if (CPU_MASK == 0xF) {
          core = (i % 4);           //core 0123
      }

      static int firstPrint = 1;
      if (firstPrint == 1) {
        firstPrint = 0;
        printf("Thread Scheduler=%s\n",
          (CODING_SCHEDULER == SCHED_FIFO)  ? "SCHED_FIFO" :
          (CODING_SCHEDULER == SCHED_RR)    ? "SCHED_RR" :
          (CODING_SCHEDULER == SCHED_OTHER) ? "SCHED_OTHER" :
          "???");
      }

      if (i < 5) {
        printf("Placing Thread%d on Core%d\n", i, core);
      } else if (i==5) {
        printf("....");
      }
      CPU_SET(core, &cpuset);
      ret = pthread_setaffinity_np(encoder_worker_threads[i], sizeof(cpuset),
                             &cpuset);
      if (ret != 0) {
        printf("pthread_setaffinity_np enc %d errno %s\n", ret, strerror(errno));
        //return ret;
      }

      // set priorities
      params.sched_priority = thread_priority_encoder;
      ret = pthread_setschedparam(encoder_worker_threads[i], CODING_SCHEDULER, &params);
      if (ret != 0) {
        printf("pthread_setschedparam with prio %d and CODING_SCHEDULER%d thread%d --> errno %s...continueing\n", thread_priority_encoder, CODING_SCHEDULER, i, strerror(errno));
        //return ret;
      }

    }
  return 0;
}

int x = 0;
struct timespec time_start, time_finish;
double elapsed;
double max=0;
double min=10000000000;
double mean=0;
double *vals;
double calculateSD(double data[])
{
    double sum = 0.0, mean, standardDeviation = 0.0;

    int i;

    for(i = 0; i < x; ++i)
    {
        sum += data[i];
    }

    mean = sum/x;

    for(i = 0; i < x; ++i)
        standardDeviation += pow(data[i] - mean, 2);

    return sqrt(standardDeviation / x);
}

static void
display_sched_attr(int policy, struct sched_param *param)
{
   printf("Main Scheduler==%s\n",
          (policy == SCHED_FIFO)  ? "SCHED_FIFO" :
          (policy == SCHED_RR)    ? "SCHED_RR" :
          (policy == SCHED_OTHER) ? "SCHED_OTHER" :
          "???");
}

int main(int argc, char **argv) {
    unsigned int abort_limit_us = ABORT_LIM;
    if (argc < 2) {
      printf("missing parameters! \n");
      printf("arm_test <THREADS> <ITERATIONS> <CPUMASK> <optional ABORTLIMIT[ms]> <optional: FORCEEXIT> <optional: NANOSLEEPMAIN> <optional: NANOSLEEPTHREAD> <optional:  SCHEDULERTHREADS (0:RR 1:FIFO 2:OTHER)> <optional: SCHEDULERMAIN (0:RR 1:FIFO 2:OTHER)>\n");
      exit(0);
    } else if (argc >= 2) {
        printf("arm_test <THREADS> <ITERATIONS> <CPUMASK> <optional ABORTLIMIT[ms]> <optional: FORCEEXIT> <optional: NANOSLEEPMAIN> <optional: NANOSLEEPTHREAD> <optional:  SCHEDULERTHREADS (0:RR 1:FIFO 2:OTHER)> <optional: SCHEDULERMAIN (0:RR 1:FIFO 2:OTHER)>\n");
        PARALLEL_ENCODING_CORES = atoi(argv[1]);
        NUM_ITERATIONS = atoi(argv[2]);
        CPU_MASK = atoi(argv[3]);
        if (argc >= 3) {
            ABORT_LIM = atoi (argv[4]);
            abort_limit_us = ABORT_LIM*1000;
        }
        if (argc >= 4) {
            FEXIT = atoi (argv[5]);
            if (FEXIT != 0) {
              FEXIT = 1;
              printf("ABORT if limit (%ums) is breached!\n", ABORT_LIM);
            }
        }
        if (argc >= 5) {
          NANOSLEEPTHREAD = atoi(argv[6]);
          if (NANOSLEEPTHREAD > 0) {
            printf("Each Thread sleeps for %dns\n", NANOSLEEPTHREAD);
          } else {
            NANOSLEEPTHREAD = 0;
            printf("Using default: Thread sleeps for %dns\n", NANOSLEEPTHREAD);
          }
        }
        if (argc >= 6) {
          NANOSLEEPMAIN = atoi(argv[7]);
          if (NANOSLEEPMAIN > 0) {
            printf("Main sleeps for %dns\n", NANOSLEEPMAIN);
          } else {
            NANOSLEEPMAIN = 0;
            printf("Using default: Main sleeps for %dns\n", NANOSLEEPMAIN);
          }
        }
        if (argc >= 7) {
          int schedthread = atoi(argv[8]);
          if (schedthread == 0) {
              CODING_SCHEDULER = SCHED_OTHER;
          }
          else if (schedthread == 1) {
              CODING_SCHEDULER = SCHED_FIFO;
          }
          else if (schedthread == 2) {
              CODING_SCHEDULER = SCHED_RR;
          } else {
            printf("Invalid Sched policy, using SCHED_RR\n");
          }
        }

        if (argc >= 8) {
          int schedmain = atoi(argv[9]);
          if (schedmain == 0) {
              CODING_SCHEDULER_MAIN = SCHED_OTHER;
          }
          else if (schedmain == 1) {
              CODING_SCHEDULER_MAIN = SCHED_FIFO;
          }
          else if (schedmain == 2) {
              CODING_SCHEDULER_MAIN = SCHED_RR;
          } else {
            printf("Invalid Sched policy, using SCHED_RR\n");
          }
        }


        if (PARALLEL_ENCODING_CORES > PARALLEL_ENCODING_CORES_MAX) {
            printf("Max 1000 Threads supported! \n”");
            return;
        }
        if (NUM_ITERATIONS <= 0) {
            printf("Only positive amount of iterations ;) \n”");
            return;
        }
        if (CPU_MASK > 0xF || CPU_MASK < 0x1) {
            printf("Mask 0xF is maximum! (4 cores)\n”");
            printf("EXAMPLES\n”");
            printf("  1  = 0b0001-> CORE 0 only\n”");
            printf("  3  = 0b0011-> CORES 0 & 1\n”");
            printf("  14 = 0b1110-> CORES 1 & 2 & 3\n”");
            return;
        }
        if (ABORT_LIM < 0 ) {
            printf("Abort limit > 0 in ms\n”");
            return;
        }
    }

    // int policy, s;
    // struct sched_param param;

    // policy = CODING_SCHEDULER_MAIN;
    // int ss = pthread_setschedparam(pthread_self(), policy, &param);
    // if (ss != 0)
    //     printf("%d", ss);
    // pthread_getschedparam(pthread_self(), &policy, &param);
    // display_sched_attr(policy, &param);

    struct sched_param sp;
    sp.sched_priority = 50;
    if(CODING_SCHEDULER_MAIN == SCHED_OTHER) {
      sp.sched_priority = 0;
    }

    pid_t pid = getpid();
    printf("sem_test pid=(%d)\n",pid);
    printf("Setting main Scheduler to %s.\n", sched_policy[CODING_SCHEDULER_MAIN]);
    int rets=sched_setscheduler(pid, CODING_SCHEDULER_MAIN, &sp);
    if (rets != 0) {
      printf("RETURN %d = %s\n\n", rets, strerror(errno));
    }
    printf("Main Scheduler is: %s.\n", sched_policy[sched_getscheduler(pid)]);

    init_coding();

    double runtime[NUM_ITERATIONS];
    vals = runtime;
    int ret;

    // usleep(50*1000);
    printf("\nAborting if %d is reached\n", ABORT_LIM);
    while (x < NUM_ITERATIONS) {
        clock_gettime(CLOCK_MONOTONIC, &time_start);
        for (int i = 0; i < PARALLEL_ENCODING_CORES; i++) {
          ret = sem_post(&encoder_lock_start[i]);
        }
        for (int i = 0; i < PARALLEL_ENCODING_CORES; i++) {
          ret = sem_wait(&encoder_lock_end);
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
        vals[x] = elapsed;
        x++;
        if (max > abort_limit_us) {
            if (FEXIT == 1) {
              exit(1);
            }
            printf("ABORT: Sem pend needed: %03fms in iteration %u\n", max/1000, x);
            break;
        }
        if (NANOSLEEPMAIN > 0) {
            nanosleep((const struct timespec[]){{0, NANOSLEEPMAIN}}, NULL);
        }

      //if (x % 100 == 0)
      //printf("round %d/%d took %f us\n", x, numIterations, elapsed);
    }
    double variance=calculateSD(vals);
    mean = mean/x;

    double runtime_limit = mean + variance;
    int cnt_above_limit=0;
    printf("\n\nTaking mean+variance = %fus + %fus = %fus as upper limit!\n", mean, variance, runtime_limit);
    for (int i = 0; i < x; i++) {
       if (vals[i] > runtime_limit) {
          cnt_above_limit++;
       }
    }

    printf("################## RESULTS ########################\n");
    printf("#Threads = %d, #Iterations= %d/%d\n", PARALLEL_ENCODING_CORES, x, NUM_ITERATIONS);
    printf("Min: %fus, Max: %fus\n", min, max);
    printf("Mean: %fus variance %fus \n", mean, variance);
    printf("Runtimes above limit(%fus): %d\n", runtime_limit, cnt_above_limit);
    printf("###################################################\n");

    for (int i = 0; i < PARALLEL_ENCODING_CORES; i++) {
      if (enc_run[i] != x) {
        printf("!! ERROR Thread%d ran %d/%d\n", i, enc_run[i], x);
      }
      if (enc_unlocked[i] != x) {
        printf("!! ERROR Thread%d unlocked %d/%d\n", i, enc_run[i], x);
      }
    }
}