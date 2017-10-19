.PHONY: all clean

CC	?= cc
CFLAGS	+= -std=c89 -Wall -pedantic
LDFLAGS	+= -lm

all:	woe

woe:	woe.c
	$(CC) -o woe woe.c $(CFLAGS) $(LDFLAGS)

clean:
	rm -f woe
