# trivial makefile for now
PROG=risu
SRCS=risu.c
OBJS=$(SRCS:.c=.o)

$(PROG): $(SRCS)
	$(CC) -g -Wall -Werror -o $@ $^

clean:
	rm -f $(PROG) $(OBJS)
