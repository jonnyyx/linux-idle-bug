CFLAGS  = -O3 -g -fno-omit-frame-pointer -W -Wextra -Wno-div-by-zero
LIBS = -lpthread -lm

TARGET = parallel-locking-stuck
SRC = $(addsuffix .c, $(TARGET))
ASM = $(addsuffix .s, $(TARGET))

TRIGGER_CMD = ./$(TARGET) 20 100000 500 0 0
RT_SETUP = taskset -c 1 chrt --fifo 99

parallel-locking-stuck: $(SRC)
	$(CC) -Wa,-ahl=$(ASM) $(CFLAGS) $< -o $@ $(LIBS)

analyze: parallel-locking-stuck
	sudo perf record -e sched:sched_switch -C 1 -- $(RT_SETUP) ./$(TRIGGER_CMD)
	@echo perf sched hist
	@echo perf script

analyze-full: parallel-locking-stuck
	sudo trace-cmd record -M 2 -p function -e sched_switch -- perf record -e sched:sched_switch -C 1 -- $(RT_SETUP) ./$(TRIGGER_CMD)
	@echo trace-cmd report 

clean:
	sudo rm -rf $(TARGET) $(ASM)
	sudo rm -rf *.data *.data.old

bootstrap:
	@echo "Install all required dependencies"
	@echo "(Debian, Fedora and co users should replace this with their counterparts)"
	sudo apt-get install trace-cmd perf-tools-unstable build-essential


all: parallel-locking-stuck 

.PHONY: clean bootstrap analyze analyze-full bootstrap
