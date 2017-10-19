.PHONY: all clean

CC	?= cc
CFLAGS	+= -static -std=c89 -Wall -pedantic
LDFLAGS	+= -lm

all:	woe

.c.o:
	@echo "CC\t$<"
	@$(CC) -c $(CFLAGS) $<

woe: woe.o
	@echo "LINK\t$@"
	@$(CC) -o $@ $< $(LDFLAGS)

clean:
	rm -f woe *.o
