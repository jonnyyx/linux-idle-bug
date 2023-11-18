CFLAGS  = -O3 -g -fno-omit-frame-pointer -O0 -Wno-div-by-zero
LIBS = -lpthread -lm

parallel-locking-stuck: parallel-locking-stuck.c
	$(CC) -Wa,-ahl=parallel-locking-stuck.s $(CFLAGS) $< -o $@ $(LIBS)

clean:
	rm -rf branching-spinner branching-spinner.s
	rm -rf *.data *.data.old

bootstrap:
	@echo "Install all required dependencies"
	@echo "(Debian, Fedora and co users should replace this with their counterparts)"


all: parallel-locking-stuck

.PHONY: clean bootstrap
