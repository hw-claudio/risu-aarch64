# trivial makefile for now
ARCH=$(shell dpkg-architecture -qDEB_HOST_ARCH_CPU)

PROG=risu
SRCS=risu.c risu_$(ARCH).c
HDRS=risu.h
OBJS=$(SRCS:.c=.o)


$(PROG): $(OBJS)
	$(CC) -g -Wall -Werror -o $@ $^

%.o: %.c $(HDRS)
	$(CC) -g -Wall -Werror -o $@ -c $<

clean:
	rm -f $(PROG) $(OBJS)
