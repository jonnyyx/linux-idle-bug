import os
import subprocess
import sys

compiler_intel = "gcc -O2 -Wall -lm -lpthread -g "
# compiler_arm = "source /opt/sdks/BLACK-DEVELOP/environment-setup-cortexa53-poky-linux; aarch64-poky-linux-gcc -fsanitize=address -lm -Wall -lpthread -O3 -Xlinker -Map=output.map  --sysroot=/opt/sdks/BLACK-DEVELOP/sysroots/cortexa53-poky-linux  "
compiler_arm = "source /opt/sdks/BLACK-DEVELOP/environment-setup-cortexa53-poky-linux; aarch64-poky-linux-gcc -fsanitize=address -lm -Wall -lpthread -O3 -g -Xlinker -Map=output.map  --sysroot=/opt/sdks/BLACK-DEVELOP/sysroots/cortexa53-poky-linux  "
requirement_tag = 'requires: '

def build_test(name, flags, compiler, out_prefix):
    file = open(name, 'r')
    lines = file.readlines()
    if len(lines) < 3:
        return
    if not lines[1].startswith(requirement_tag):
        print('requirement tag not found')
        #return
    requires = lines[1].replace(requirement_tag,'').replace('\n','').replace('\r','')
    print(requires)
    c_sources = name + ' ' + requires
    cmd = compiler + ' ' + c_sources + ' ' + flags + ' -o ' + out_prefix + name.replace('.c','')
    print(cmd)
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True, executable='/bin/bash')
    output, error = process.communicate()
    print(output)
    print(error)


if __name__ == "__main__":
    #build_test('testTurboRoundtrip.c', '', compiler_intel, '')
    #build_test('testTurboRoundtrip.c', '', compiler_arm, 'arm_')
    #build_test('test.c', '', compiler_intel, '')
    build_test('test.c', '', compiler_arm, 'arm_')