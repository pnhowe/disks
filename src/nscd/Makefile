CC = gcc
CFLAGS = -Wall -Wl,--sort-section -Wl,alignment -Wl,--sort-common -Os -fomit-frame-pointer $(EXTRA_CFLAGS)

all: nscd

nscd.c: nscd-0.54.c
	ln -sf nscd-0.54.c nscd.c

nscd: nscd.c

clean:
	rm -f nscd.o nscd nscd.c

test-requires:

test:

lint:

.PHONY: clean test-requires test lint
