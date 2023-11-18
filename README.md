# build scripts:
- `$ python build.py native|aarch64poky|cortexa53poky`
- generated files in build/

# run tests
## help:
- `$ ./scripts/test_{native|...|...}` 
Output:
```
root@sdtr-black-develop:/home# ./swapper-test-cortexa53poky
Missing parameters!
PLease provide the following parameteres:
<numthreads>             number of worker threads
<iterations>             number of iterations
<cpumask>                cpu mask, to place threads
<abortlimit>             Abort in us if measurement exceeds this value
<forceexit>              Exit(1) without printing summary
<nanosleepthread>        Sleep in Thread during in work part
<nanosleepmain>          Sleep in main after iteration
<schedthread>            Scheduler of worker threads --> 0=OTHER, 1=FIFO, 2=RR
<schedmain>              Scheduler of main thread --> 0=OTHER, 1=FIFO, 2=RR
<locktype>               Type of test, 0=Semaphores, 1=Barriers
```

- `chmod +x <executablefile>`
- enable required cores, if multiple cores shall be used during the test:
    - `echo 1 > /sys/devices/system/cpu/cpu1/online`
    - `echo 1 > /sys/devices/system/cpu/cpu2/online`
    - `echo 1 > /sys/devices/system/cpu/cpu3/online` ...
- the main() which is also a "worker" in the test is placed by the kernel. if a explicit core shall be used:
    - `$ taskset <MASK> testexecutable ....`
      with <MASK:1|2|4|8|...> a integer bitmask for the Core, where the executed `testexecutable` shall run
      - 1 = 0b000**1** --> CPU0
      - 2 = 0b00**1**0 --> CPU1
      - 4 = 0b0**1**00 --> CPU2
      - 8 = 0b**1**000 --> CPU3

## run test single core
```
root@sdtr-black-develop:/home# taskset 0x2 ./swapper-test-cortexa53poky 5 100000 2 1000 0 0 100000 2 2 0

-----------------------------
Summary before start:
<numthreads>             5
<iterations>             100000
<cpumask>                2
<abortlimit>             1000us
<forceexit>              0
<nanosleepthread>        0ns
<nanosleepmain>          100000ns
<schedthread>            SCHED_RR
<schedmain>              SCHED_RR
<locktype>               SEMAPHORES
-----------------------------
Placing Thread0 on Core1
Placing Thread1 on Core1
Placing Thread2 on Core1
Placing Thread3 on Core1
Placing Thread4 on Core1
Aborting if time >1.000ms is measured
################## SUMMARY ########################
Type of test: SEMAPHORES
Threads: 5               Iterations: 100000/100000
Min: 89.000000us,        Max: 208.000000us
Mean: 90.482460us        Variance: 1.159678us
###################################################
```

## run test multiple cores
```
root@sdtr-black-develop:/home# echo 1 > /sys/devices/system/cpu/cpu2/online
root@sdtr-black-develop:/home# ./swapper-test-cortexa53poky 5 100000 5 1000 0 0 100000 2 2 0

-----------------------------
Summary before start:
<numthreads>             5
<iterations>             100000
<cpumask>                5
<abortlimit>             1000us
<forceexit>              0
<nanosleepthread>        0ns
<nanosleepmain>          100000ns
<schedthread>            SCHED_RR
<schedmain>              SCHED_RR
<locktype>               SEMAPHORES
-----------------------------
Placing Thread0 on Core0
Placing Thread1 on Core2
Placing Thread2 on Core0
Placing Thread3 on Core2
Placing Thread4 on Core0
```

# provoke failure
To provoke failures, the <nanosleepmain> and/or <nanosleep> should be rather low or even 0. Thus, the programm creates high cpu load.
```
root@sdtr-black-develop:/home# echo 1 > /sys/devices/system/cpu/cpu1/online
root@sdtr-black-develop:/home# taskset 0x2 ./swapper-test-cortexa53poky 10 100000 2 1000 0 0 0 2 2 0
-----------------------------
Summary before start:
<numthreads>             10
<iterations>             100000
<cpumask>                2
<abortlimit>             1000us
<forceexit>              0
<nanosleepthread>        0ns
<nanosleepmain>          0ns
<schedthread>            SCHED_RR
<schedmain>              SCHED_RR
<locktype>               SEMAPHORES
-----------------------------
Placing Thread0 on Core1
Placing Thread1 on Core1
Placing Thread2 on Core1
Placing Thread3 on Core1
Placing Thread4 on Core1
....
Aborting if time >1.000ms is measured
ABORT: SEMAPHORES-Iteration needed: 50.229ms in iteration 5068

################## SUMMARY ########################
Type of test: SEMAPHORES
Threads: 10              Iterations: **5068**/100000 --> the program aborted
Min: 180.000000us,       Max: 50229.000000us --> 50ms was measured
Mean: 195.385556us       Variance: 702.891287us
###################################################
```

## perf
Measure an incided with perf:
```
perf record -C 1 -e sched:sched_switch,sched:sched_waking,raw_syscalls:sys_enter,raw_syscalls:sys_exit -- taskset 0x2 ./swapper_test_<..> 1 100000 2 1 0 0 0 2 2
```
- with -C 1, the core (here 1 because of taskset 0x2 for main thread and bitmask 0b0010=2 for worker threads), where perf shall monitor SYS_CALLS, no -C --> all cores are monitored
- <abortlimit> set, such that the programm terminates in the failure case
- <forcexit> 1 to reduce output and thus perf records in the log afterwards. Also possible with 0 though.

after the program terminated with a failure either by

    - Iterations: **5068**/100000, if <forcexit> = 0
    - No Output at all if <forceexit> = 1

run `$ perf sched timehist`, jump to the end of the log, and scroll upwards **using the arrows on the keyboard** until you find:
```
8251.635753 [0001]  swapper_test_<..>[4505]  0.009     0.000    0.011
8251.635763 [0001]  workerthread0[4507/4505] 0.011     0.005    0.009
8251.635783 [0001]  swapper_test_<..>[4505]  0.009     0.000    0.020
8251.635833 [0001]  kcompactd0[37]           0.000     880.478  0.049
8251.635937 [0001]  ksoftirqd/1[17]          956.305   872.535  0.103
8251.685805 [0001]  <idle>                   972.016   0.000    49.868 -----> Idle task runs49.868ms!
8251.685820 [0001]  workerthread0[4507/4505] 50.042    50.035   0.015  -----> Thread0 wants to work since 50.035ms
8251.688487 [0001]  swapper_test_<..>[4505]  50.036    0.000    2.667
```
Then copy the timestamp of one of these lines eg. 8251.685805 of <idle> type `q` and then `perf script`-
In the log of `perf script` type `/8251.685805` then `enter` to search for this timestamp (may take a while!)

When found, feel three to analyze the detailed log:
```
workerthread0  4507 [001]       8251.635763:   sched:sched_switch: prev_comm=workerthread0 prev_pid=4507 prev_prio=24 prev_state=S ==> next_comm=swapper_test_<..> next_pid=4505 next_prio=49

swapper_test_<..>  4505 [001]   8251.635767:   raw_syscalls:sys_enter: NR 98 (55626607e0, 81, 1, 0, 0, 203b)
swapper_test_<..>  4505 [001]   8251.635769:   sched:sched_waking: comm=workerthread0 pid=4507 prio=24 target_cpu=001
swapper_test_<..>  4505 [001]   8251.635772:   raw_syscalls:sys_exit: NR 98 = 1
swapper_test_<..>  4505 [001]   8251.635783:   sched:sched_switch: prev_comm=swapper_test_<..> prev_pid=4505 prev_prio=49 prev_state=R ==> next_comm=kcompactd0 next_pid=37 next_prio=120

kcompactd0    37 [001]          8251.635833:   sched:sched_switch: prev_comm=kcompactd0 prev_pid=37 prev_prio=120 prev_state=S ==> next_comm=ksoftirqd/1 next_pid=17 next_prio=120

ksoftirqd/1    17 [001]         8251.635861:   sched:sched_waking: comm=kworker/u8:1 pid=4123 prio=120 target_cpu=000
ksoftirqd/1    17 [001]         8251.635937:   sched:sched_switch: prev_comm=ksoftirqd/1 prev_pid=17 prev_prio=120 prev_state=S ==> next_comm=swapper/1 next_pid=0 next_prio=120

swapper     0 [001]             8251.685805:   sched:sched_switch: prev_comm=swapper/1 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=workerthread0 next_pid=4507 next_prio=24 --> Swapper = <idle> ran for 50ms here!

workerthread0  4507 [001]       8251.685808:   raw_syscalls:sys_exit: NR 98 = 0
workerthread0  4507 [001]       8251.685814:   raw_syscalls:sys_enter: NR 98 (55626607e0, 189, 0, 0, 0, ffffffff)
workerthread0  4507 [001]       8251.685820:   sched:sched_switch: prev_comm=workerthread0 prev_pid=4507 prev_prio=24 prev_state=S ==> next_comm=swapper_test_<..> next_pid=4505 next_prio=49

swapper_test_<..>  4505 [001]   8251.685897:   raw_syscalls:sys_enter: NR 64 (1, 7fb5203780, 37, 0, 55626454af, 7fb52037b7)
```
