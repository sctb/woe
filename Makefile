PROG	= woe

CC	?= cc
CFLAGS	+= -std=c89 -Wall -pedantic

all:	options $(PROG)

options:
	@echo "CFLAGS\t= $(CFLAGS)"
	@echo "LDFLAGS\t= $(LDFLAGS)"
	@echo "CC\t= $(CC)\n"

SRC	= woe.c
OBJ	= woe.o

.c.o:
	@echo "CC\t$<"
	@$(CC) -c $(CFLAGS) $<

$(PROG): $(OBJ)
	@echo "LINK\t$@"
	@$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(PROG) $(OBJ)

.PHONY: all options clean
