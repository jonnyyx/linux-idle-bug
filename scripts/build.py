import os
import subprocess
import sys
import argparse

compiler_native = "gcc"
compiler_aarch = "aarch64-poky-linux-gcc"
compiler_cortexa = "aarch64-poky-linux-gcc"
requirement_tag = "requires: "

def build_test(c_sources, compileflags, linkflags, sourcescript, compiler, target, build_path):
    file = open(c_sources, "r")
    lines = file.readlines()
    if len(lines) < 3:
        return
    if not lines[1].startswith(requirement_tag):
        print("requirement tag not found")
        return

    requires = lines[1].replace(requirement_tag,"").replace("\n","").replace("\r","")
    cmd = sourcescript + " " + compiler + " " + compileflags + " " + requires + " " + c_sources + " -o " + build_path + "swapper-test-" + target + " " + linkflags
    print("Build command: ", cmd)
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True, executable="/bin/bash")
    output, error = process.communicate()
    
    if (len(output) > 0):
        print(output)
    if (error != None):
        print(error)
    else:
        print("No compile errors")

if __name__ == "__main__":    
    parser = argparse.ArgumentParser(description="Build script linux-swapper-bug")
    parser.add_argument("target", choices = ["native", "aarch64poky", "cortexa53poky"],
                        help="Target { native | aarch64poky | cortexa53poky }")
    args = parser.parse_args()
    target = args.target
    
    sourcescript = ""
    flags = ""
    linkflags = ""
    if "native" in target:
        compileflags = "-O2 -Wall -g"
        linkflags = "-lm -lpthread"
        compiler = compiler_native
    if "aarch" in target:
        compileflags = "-fsanitize=address -lm -Wall -lpthread -O3 -Xlinker -Map=output.map --sysroot=/opt/sdhr/1.6.0/sysroots/aarch64-poky-linux"
        sourcescript = "source /opt/sdhr/1.6.0/environment-setup-aarch64-poky-linux;"
        compiler = compiler_aarch
    if "cortexa" in target:
        compileflags = "-fsanitize=address -lm -lpthread -Wall -O3 -g -Xlinker -Map=output.map  --sysroot=/opt/sdks/BLACK-DEVELOP/sysroots/cortexa53-poky-linux"
        sourcescript = "source /opt/sdks/BLACK-DEVELOP/environment-setup-cortexa53-poky-linux;"
        compiler = compiler_cortexa

    dir = os.path.abspath(os.path.dirname(__file__))
    src_dir = os.path.join(dir, "../src/")
    build_path = os.path.join(dir, "../build/")

    build_test(f"{src_dir}swapper-test.c", compileflags, linkflags, sourcescript, compiler, target, build_path)