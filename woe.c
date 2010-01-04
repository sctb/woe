#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define w_spacep(x) (x == ' ' || x == '\t')

enum w_token_type {
	/* factors */
	WT_STRING,
	WT_FIXNUM,
	WT_FLONUM,
	WT_LSQUARE,
	WT_SYMBOL,

	/* other */
	WT_COLON,
	WT_SEMICOL,
	WT_RSQUARE,

	/* terminators */
	WT_EOL,
	WT_EOF
};

struct w_token {
	enum w_token_type type;
	union {
		long	fixnum;
		double	flonum;
		char 	*string;
	} value;
};

struct w_reader {
	FILE 	*stream;
	int 	buflen;
	int	bufpos;
	char 	buffer[1024];

	/* unused */
	int 	line;
	int 	column;
};

enum w_tag {
	W_BOOL,
	W_STRING,
	W_FIXNUM,
	W_FLONUM,
	W_QUOT
};

union w_value {
	long 	fixnum;
	double 	flonum;
	char 	*string;
	struct 	w_node 	*node;
	struct 	w_entry	*entry;
	void(*proc)();
};

struct w_node {
	enum 	w_tag 	tag;
	union 	w_value	value;
	struct 	w_node 	*next;
};

struct w_entry {
	char 	*name;
	union {
		struct w_node *body;
		void(*proc)();
	} value;
	struct 	w_entry	*next;
};

void
w_init_reader(struct w_reader *r, FILE *stream)
{
	r->stream	= stream;
	r->line 	= 1;
	r->column 	= 0;
	r->buflen	= 0;
	r->bufpos	= 0;
}

char
w_read_char(struct w_reader *r)
{
	char c;

	if (r->bufpos == r->buflen)
	{
		if (fgets(r->buffer, 1024, r->stream) == NULL)
			return ('\0');
		r->buflen = strlen(r->buffer);
		r->bufpos = 0;
	}

	c = r->buffer[r->bufpos++];

	if (c == '\n')
	{
		r->line++;
		r->column = 0;
	}

	return (c);
}

void
w_unread_char(struct w_reader *r)
{
	r->bufpos--;
	r->column--;
}

struct w_token
w_read_string(struct w_reader *r)
{
	size_t 	pos;
	char 	c;
	char 	buffer[1024];
	struct	w_token t;

	pos 	= 0;
	t.type 	= WT_STRING;

	while ((c = w_read_char(r)) != '"') {
		if (c == '\\') {
			buffer[pos++] = c;
			c = w_read_char(r);
		}
		buffer[pos++] = c;
	}

	buffer[pos++] = '\0';
	t.value.string = (char*)malloc(pos);
	strncpy(t.value.string, buffer, pos);

	return (t);
}

struct w_token
w_read_number(struct w_reader *r)
{
	size_t 	pos;
	char 	c;
	char 	buffer[1024];
	struct	w_token t;

	pos 	= 0;
	t.type 	= WT_SYMBOL;

	while ((c = w_read_char(r)) != '\0')
	{
		if (!strchr("0123456789+-", c))
		{
			if (strchr(".eE", c))
				t.type = WT_FLONUM;
			else {
				w_unread_char(r);

				if (pos > 1 || (pos == 1 && buffer[0] != '-'))
					break;
				else
					return (t);
			}
		}
		buffer[pos++] = c;
	}

	buffer[pos + 1] = '\0';

	if (t.type == WT_FLONUM)
		t.value.flonum = strtod(buffer, NULL);
	else {
		t.type = WT_FIXNUM;
		t.value.fixnum = strtol(buffer, NULL, 10);
	}

	return (t);
}

struct w_token
w_read_symbol(struct w_reader *r)
{
	size_t 	pos;
	char 	c;
	char 	buffer[1024];
	struct	w_token t;

	pos 	= 0;
	t.type	= WT_SYMBOL;

	while ((c = w_read_char(r)) != '\0')
	{
		if (w_spacep(c) || c == '\n')
		{
			w_unread_char(r);
			break;
		} else
			buffer[pos++] = c;
	}

	buffer[pos++] = '\0';
	t.value.string = (char*)malloc(pos);
	strncpy(t.value.string, buffer, pos);

	return (t);
}


struct w_token
w_read_token(struct w_reader *r)
{
	char 	c;
	struct 	w_token t;

restart:
	do {
		c = w_read_char(r);
	} while (w_spacep(c));

	if (c == '(')
	{
		do {
			c = w_read_char(r);
		} while (c != ')');
		goto restart;
	}

	switch (c)
	{
	case '\n':
		t.type = WT_EOL;
		return t;
	case '\0':
		t.type = WT_EOF;
		return t;
	case '[':
		t.type = WT_LSQUARE;
		return t;
	case ']':
		t.type = WT_RSQUARE;
		return t;
	case ':':
		t.type = WT_COLON;
		return t;
	case ';':
		t.type = WT_SEMICOL;
		return t;
	case '"':
		return w_read_string(r);
	case '-':
		w_unread_char(r);
		if ((t = w_read_number(r)).type != WT_SYMBOL)
			return t;
	default:
		w_unread_char(r);
		if (isdigit(c))
			t = w_read_number(r);
		else
			t = w_read_symbol(r);
		return t;
	}
}

int
main(int argc, char *argv[])
{
	struct w_token 	t;
	struct w_reader	r;

	w_init_reader(&r, stdin);

prompt:
	printf("OK ");

	while (1)
	{
		switch ((t = w_read_token(&r)).type)
		{
		case WT_EOF:
			return (0);
		case WT_EOL:
			goto prompt;
		case WT_LSQUARE:
			printf("LSQUARE\n");
			break;
		case WT_RSQUARE:
			printf("RSQUARE\n");
			break;
		case WT_COLON:
			printf("COLON\n");
			break;
		case WT_SEMICOL:
			printf("SEMICOL\n");
			break;
		case WT_STRING:
			printf("STRING: %s\n", t.value.string);
			free(t.value.string);
			break;
		case WT_FIXNUM:
			printf("FIXNUM: %ld\n", t.value.fixnum);
			break;
		case WT_FLONUM:
			printf("FLONUM: %.2f\n", t.value.flonum);
			break;
		case WT_SYMBOL:
			printf("SYMBOL: %s\n", t.value.string);
			free(t.value.string);
			break;
		}
	}

	return (0);
}
