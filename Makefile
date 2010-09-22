# trivial makefile for now
ARCH=$(shell dpkg-architecture -qDEB_HOST_ARCH_CPU)

PROG=risu
SRCS=risu.c comms.c risu_$(ARCH).c
HDRS=risu.h
BINS=test_$(ARCH).bin

OBJS=$(SRCS:.c=.o)

all: $(PROG) $(BINS)

$(PROG): $(OBJS)
	$(CC) -g -Wall -Werror -o $@ $^

%.o: %.c $(HDRS)
	$(CC) -g -Wall -Werror -o $@ -c $<

%_i386.bin: %_i386.s
	nasm -f bin -o $@ $<

%_arm.bin: %_arm.elf
	objcopy -O binary $< $@

%_arm.elf: %_arm.s
	as -o $@ $<

clean:
	rm -f $(PROG) $(OBJS) $(BINS)
