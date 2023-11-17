# build scripts:
- `$ python build.py native|aarch64poky|cortexa53poky`
- generated files in build/

# run test
- `$ ./scripts/test_{native|...|...}` 
Output:
```

```

# run test on target
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

# Results 
```
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

```