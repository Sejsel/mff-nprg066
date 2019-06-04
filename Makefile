CFLAGS=-std=c11 -Wall -Wextra -Wpedantic -g -O2
LIBS=

.PHONY: all test clean

all: ed

ed: src/ed.c
	gcc $(CFLAGS) -o $@ $^ $(LIBS)

test: ed
	cd tests/phase-1; export ED=../../ed; ../../stef/stef.sh;

clean:
	rm -f *.o ed
