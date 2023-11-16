import subprocess
import sys

cmd = "gcc -O3 ../src/test.c -lpthread -lm"
process = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True, executable='/bin/bash')
output, error = process.communicate()
print(output)
print(error)