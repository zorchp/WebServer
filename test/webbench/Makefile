CFLAGS?=-Wall -O2
CC?=gcc
LIBS?=
LDFLAGS?=

.PHONY: clean install all tar

all: 
	$(CC)  webbench.c $(CFLAGS) -o webbench.out

-clean:
	rm webbench.out
