#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define w_spacep(x) (x == ' ' || x == '\t')

#define w_starts_atomp(x) ((x).type <= WT_SYMBOL)

#define w_make_atom(typ,val,stk,t,n)			\
	do {						\
		n->type		= typ;			\
		n->value.val	= t.value.val;		\
		n->next		= stk;			\
	} while (0);

enum w_token_type {
	WT_STRING,
	WT_FIXNUM,
	WT_FLONUM,
	WT_LSQUARE,
	WT_SYMBOL,

	WT_COLON,
	WT_SEMICOL,
	WT_RSQUARE,

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

enum w_type {
	W_STRING,
	W_FIXNUM,
	W_FLONUM,
	W_QUOT,
	W_SYMBOL
};

union w_value {
	long 	fixnum;
	double 	flonum;
	char 	*string;
	struct 	w_node 	*node;
};

struct w_node {
	enum 	w_type 	type;
	union 	w_value	value;
	struct 	w_node 	*next;
};

struct w_word {
	char 		*name;
	struct w_node 	*quot;
	struct w_node	*(*builtin)(struct w_node*);
	struct w_word	*next;
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
		return (t);
	case '\0':
		t.type = WT_EOF;
		return (t);
	case '[':
		t.type = WT_LSQUARE;
		return (t);
	case ']':
		t.type = WT_RSQUARE;
		return (t);
	case ':':
		t.type = WT_COLON;
		return (t);
	case ';':
		t.type = WT_SEMICOL;
		return (t);
	case '"':
		return (w_read_string(r));
	case '-':
		w_unread_char(r);
		if ((t = w_read_number(r)).type != WT_SYMBOL)
			return (t);
	default:
		w_unread_char(r);
		if (isdigit(c))
			t = w_read_number(r);
		else
			t = w_read_symbol(r);
		return (t);
	}
}

struct w_node*
w_read_quot(struct w_reader*, struct w_node*, struct w_node*);

struct w_node*
w_read_atom(struct w_reader *r, struct w_node *stack, struct w_token t)
{
	struct w_node *n;

	n = (struct w_node*)malloc(sizeof(struct w_node));

	switch (t.type)
	{
	case WT_FIXNUM:
		w_make_atom(W_FIXNUM, fixnum, stack, t, n)
		break;
	case WT_FLONUM:
		w_make_atom(W_FLONUM, flonum, stack, t, n)
		break;
	case WT_STRING:
		w_make_atom(W_STRING, string, stack, t, n)
		break;
	case WT_SYMBOL:
		w_make_atom(W_SYMBOL, string, stack, t, n)
		break;
	case WT_LSQUARE:
		n = w_read_quot(r, stack, n);
		break;
	default:
		return (NULL);
	}

	return (n);
}

struct w_node*
w_read_quot(struct w_reader *r, struct w_node *stack, struct w_node *n)
{
	struct w_token 	t;

	n->value.node 	= NULL;
	n->type		= W_QUOT;
	n->next 	= stack;

	while (w_starts_atomp(t = w_read_token(r)))
	{
		n->value.node = w_read_atom(r, n->value.node, t);
	}

	return (n);
}

void
w_print(struct w_node* n)
{
	switch (n->type)
	{
	case W_STRING:
		printf("\"%s\" ", n->value.string);
		break;
	case W_FIXNUM:
		printf("%ld ", n->value.fixnum);
		break;
	case W_FLONUM:
		printf("%.2f ", n->value.flonum);
		break;
	case W_QUOT:
		printf("[ ");
		w_print(n->value.node);
		printf("] ");
		break;
	case W_SYMBOL:
		printf("%s ", n->value.string);
		break;
	}

	if (n->next != NULL)
		w_print(n->next);
}

int
main(int argc, char *argv[])
{
	struct w_token 	t;
	struct w_reader	r;
	struct w_node	*stack;

	w_init_reader(&r, stdin);

prompt:
	stack = NULL;
	printf("OK ");

	while (1)
	{
		switch ((t = w_read_token(&r)).type)
		{
		case WT_EOF:
			return (0);
		case WT_EOL:
			if (stack != NULL)
			{
				printf("( ");
				w_print(stack);
				printf(")\n");
			}
			goto prompt;
		default:
			stack = w_read_atom(&r, stack, t);
		}
	}

	return (0);
}
