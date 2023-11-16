#build VM:
$ python build_vm.py

#build ARM
$ python build_arm.py --> need to set rights later to execute!


enable all cores:
echo 1 > /sys/devices/system/cpu/cpu1/online
echo 1 > /sys/devices/system/cpu/cpu2/online
echo 1 > /sys/devices/system/cpu/cpu3/online


How to use:
#VM: 
$ ./a.out <THREADS> <ITERATIONS> <CPUMASK> 
with THREADS = number of threads
with ITERATIONS = number of iterations
with CPUMASK = bitmask to choose the desired CPUS the threads will execute. 
1 = 0b0001 --> CPU0
2 = 0b0010 --> CPU1
3 = 0b0011 --> CPU1 & CPU2
...
14 = 0b1110 --> CPU1 & CPU2 & CPU3
15 = 0b1111 --> ALL 4 CPUS

#ARM
Same as on VM.
but CALL with $ taskset CPU_MAIN_MASK /home/arm_test 20 5000 14
with CPU_MAIN_MASK = bitmask to chose where the MAIN thread runs!
1 = 0b0001 --> CPU0
2 = 0b0010 --> CPU1
4 = 0b0100 --> CPU2
8 = 0b1000 --> CPU3


OUTPUT
root@zynqmp-sdhr:~# taskset 0x1 /home/arm_test 20 5000 14    <<<<< MAIN THREAD ON CPU0! (where the kernel runs), 14=0b1110 --> CPU123
Creating 20 Threads!
Placing Thread0 on Core1
Placing Thread1 on Core2
Placing Thread2 on Core3
Placing Thread3 on Core1 <<<<<< DEBUG stuff to check placement of Threads....
Placing Thread4 on Core2
....

Taking mean+variance = 133.922800us + 433.608221us = 567.531021us as upper limit!
################## RESULTS ########################
#Threads = 20, #Iterations= 5000
Min: 55.000000us, Max: 30714.000000us        Max value is measured with 30ms.
Mean: 133.922800us variance 433.608221us
Rundtimes above limit(567.531021us): 1
###################################################



root@zynqmp-sdhr:~# taskset 0x2 /home/arm_test 20 5000 14	<<<<< MAIN THREAD ON CPU1! (NOT where the kernel runs), 14=0b1110 --> CPU123
Creating 20 Threads!
Placing Thread0 on Core1
Placing Thread1 on Core2
Placing Thread2 on Core3
Placing Thread3 on Core1
Placing Thread4 on Core2
....

Taking mean+variance = 97.232200us + 5.649733us = 102.881933us as upper limit!
################## RESULTS ########################
#Threads = 20, #Iterations= 5000
Min: 95.000000us, Max: 264.000000us      <<<< MAX is WAY lower than in previous example!!!
Mean: 97.232200us variance 5.649733us
Rundtimes above limit(102.881933us): 133
###################################################

