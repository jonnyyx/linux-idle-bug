CFLAGS  = -O3 -g -fno-omit-frame-pointer -O0 -Wno-div-by-zero
LIBS = -lpthread -lm

parallel-locking-stuck: parallel-locking-stuck.c
	$(CC) -Wa,-ahl=parallel-locking-stuck.s $(CFLAGS) $< -o $@ $(LIBS)

test: parallel-locking-stuck
	sudo perf record -e sched:sched_switch -C 1 -- taskset -c 1 chrt --fifo 99 ./parallel-locking-stuck 20 100000 500 0 0

clean:
	rm -rf branching-spinner branching-spinner.s
	rm -rf *.data *.data.old

bootstrap:
	@echo "Install all required dependencies"
	@echo "(Debian, Fedora and co users should replace this with their counterparts)"


all: parallel-locking-stuck

.PHONY: clean bootstrap
