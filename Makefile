# trivial makefile for now
ARCH=$(shell dpkg-architecture -qDEB_HOST_ARCH_CPU)

PROG=risu
SRCS=risu.c comms.c risu_$(ARCH).c
HDRS=risu.h
BINS=test_i386.bin

OBJS=$(SRCS:.c=.o)

all: $(PROG) $(BINS)

$(PROG): $(OBJS)
	$(CC) -g -Wall -Werror -o $@ $^

%.o: %.c $(HDRS)
	$(CC) -g -Wall -Werror -o $@ -c $<

%.bin: %.s
	nasm -f bin -o $@ $<

clean:
	rm -f $(PROG) $(OBJS) $(BINS)
