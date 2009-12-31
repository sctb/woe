CC	?= cc

CFLAGS	+= -Wall -ansi -pedantic

all:	options woe

options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "CC      = ${CC}"
	@echo ""

woe:
	@echo CC -o $@
	@${CC} $@.c -o $@ ${LDFLAGS}

clean:
	rm -f woe

.PHONY: all options clean
