.PHONY: all clean

CC	?= cc
CFLAGS	+= -std=c89 -Wall -pedantic

all:	woe

.c.o:
	@echo "CC\t$<"
	@$(CC) -c $(CFLAGS) $<

woe: woe.o
	@echo "LINK\t$@"
	@$(CC) -o $@ $< $(LDFLAGS)

clean:
	rm -f woe *.o
