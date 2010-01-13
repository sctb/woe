#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W_SPACEP(x) (x == ' ' || x == '\t')
#define W_STARTS_ATOMP(x) ((x).type <= WT_SYMBOL)

#define W_MAKE_NODE(x, typ, fld, val)				\
	struct w_node *x;					\
	x		= w_alloc_node();			\
	x->type		= typ;					\
	x->value.fld	= val;					\

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
		char	*string;
	} value;
};

struct w_reader {
	FILE	*stream;
	int	buflen;
	int	bufpos;
	char	buffer[1024];

	/* unused */
	int	line;
	int	column;
};

enum w_type {
	W_STRING,
	W_FIXNUM,
	W_FLONUM,
	W_QUOT,
	W_SYMBOL
};

union w_value {
	long	fixnum;
	double	flonum;
	char	*string;
	struct	w_node	*node;
};

struct w_node {
	enum	w_type	type;
	union	w_value	value;
	struct	w_node	*next;
};

struct w_word {
	char		*name;
	struct w_node	*(*builtin)(struct w_node*);
	struct w_node	*quot;
	struct w_word	*next;
};

struct w_env {
	struct w_node *data;
	struct w_node *code;
	struct w_word *dict;
};

struct w_builtin {
	char		*name;
	struct w_node	*(*builtin)(struct w_node*);
};

struct w_node*
w_alloc_node()
{
	return ((struct w_node*)malloc(sizeof(struct w_node)));
}

struct w_word*
w_alloc_word()
{
	return ((struct w_word*)malloc(sizeof(struct w_word)));
}

char*
w_alloc_string(size_t len)
{
	return ((char*)malloc(sizeof(char)*len));
}

struct w_node*
w_copy_node(struct w_node *o)
{
	struct w_node *n;

	n		= w_alloc_node();
	n->type		= o->type;
	n->value	= o->value;

	return (n);
}

void
w_init_reader(struct w_reader *r, FILE *stream)
{
	r->stream	= stream;
	r->line		= 1;
	r->column	= 0;
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
	size_t	pos;
	char	c;
	char	buffer[1024];
	struct	w_token t;

	pos	= 0;
	t.type	= WT_STRING;

	while ((c = w_read_char(r)) != '"') {
		if (c == '\\') {
			buffer[pos++] = c;
			c = w_read_char(r);
		}
		buffer[pos++] = c;
	}

	buffer[pos++] = '\0';
	t.value.string = w_alloc_string(pos);
	strncpy(t.value.string, buffer, pos);

	return (t);
}

struct w_token
w_read_number(struct w_reader *r)
{
	size_t	pos;
	char	c;
	char	buffer[1024];
	struct	w_token t;

	pos	= 0;
	t.type	= WT_SYMBOL;

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

	buffer[pos] = '\0';

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
	size_t	pos;
	char	c;
	char	buffer[1024];
	struct	w_token t;

	pos	= 0;
	t.type	= WT_SYMBOL;

	while ((c = w_read_char(r)) != '\0')
	{
		if (W_SPACEP(c) || c == '\n')
		{
			w_unread_char(r);
			break;
		} else
			buffer[pos++] = c;
	}

	buffer[pos++] = '\0';
	t.value.string = w_alloc_string(pos);
	strncpy(t.value.string, buffer, pos);

	return (t);
}

struct w_token
w_read_token(struct w_reader *r)
{
	char	c;
	struct	w_token t;

restart:
	do {
		c = w_read_char(r);
	} while (W_SPACEP(c));

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
w_make_fixnum(long value)
{
	W_MAKE_NODE(n, W_FIXNUM, fixnum, value);

	return (n);
}

struct w_node*
w_make_flonum(float value)
{
	W_MAKE_NODE(n, W_FLONUM, flonum, value);

	return (n);
}

struct w_node*
w_make_string(char *value)
{
	W_MAKE_NODE(n, W_STRING, string, value);

	return (n);
}

struct w_node*
w_make_symbol(char *value)
{
	W_MAKE_NODE(n, W_SYMBOL, string, value);

	return (n);
}

struct w_node*
w_extend(struct w_node *n, struct w_node **h, struct w_node **o)
{
	if (*h == NULL)
		*h = n;
	if (*o == NULL)
		*o = n;
	else
		(*o)->next = n;

	return (n);
}

struct w_node*
w_read_quot(struct w_reader*);

struct w_node*
w_read_atom(struct w_reader *r, struct w_token t)
{
	switch (t.type)
	{
	case WT_FIXNUM:
		return (w_make_fixnum(t.value.fixnum));
	case WT_FLONUM:
		return (w_make_flonum(t.value.flonum));
	case WT_STRING:
		return (w_make_string(t.value.string));
	case WT_SYMBOL:
		return (w_make_symbol(t.value.string));
	case WT_LSQUARE:
		return (w_read_quot(r));
	default:
		return (NULL);
	}
}

struct w_node*
w_read_quot(struct w_reader *r)
{
	struct w_token	t;
	struct w_node	*n;
	struct w_node	*l;

	n	= w_alloc_node();
	n->type	= W_QUOT;

	while (W_STARTS_ATOMP(t = w_read_token(r)))
		l = w_extend(w_read_atom(r, t), &n->value.node, &l);

	return (n);
}

struct w_word*
w_lookup(struct w_word *w, char *name)
{
	while (w != NULL) {
		if (strcasecmp(w->name, name) == 0)
			return (w);
		w = w->next;
	}

	return (NULL);
}

struct w_node*
w_swap(struct w_node *o)
{
	struct w_node *n;

	n	= o->next;
	o->next	= n->next;
	n->next	= o;

	return (n);
}

struct w_node*
w_dup(struct w_node *o)
{
	struct w_node *n;

	n		= w_copy_node(o);
	n->next		= o;

	return (n);
}

struct w_node*
w_zap(struct w_node *n)
{
	return (n->next);
}

void
w_print_node(struct w_node *n);

struct w_node*
w_print(struct w_node *n)
{
	if (n == NULL)
		return (n);

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
		w_print_node(n->value.node);
		printf("] ");
		break;
	case W_SYMBOL:
		printf("%s ", n->value.string);
		break;
	}

	return (n);
}

struct w_builtin initial_dict[] = {
	{ "SWAP",	w_swap	},
	{ "DUP",	w_dup	},
	{ "ZAP",	w_zap	},
	{ "PRINT",	w_print	}
};

void
w_print_node(struct w_node *n)
{
	while (n != NULL) {
		w_print(n);
		n = n->next;
	}
}

struct w_word*
w_make_builtin_dict()
{
	int i, len;
	struct w_word		*p;
	struct w_word		*w;
	struct w_builtin	*d;

	len	= sizeof(initial_dict) / sizeof(initial_dict[0]);
	d	= initial_dict;
	p	= NULL;

	for (i = 0; i < len; i++) {
		w		= w_alloc_word();
		w->name		= d[i].name;
		w->builtin	= d[i].builtin;
		w->next		= p;
		p		= w;
	}

	return (w);
}

void
w_init_env(struct w_env *e)
{
	e->data = NULL;
	e->code = NULL;
	e->dict = w_make_builtin_dict();
}

struct w_node*
w_call(struct w_word *w, struct w_node *d)
{
	if (w->builtin != NULL)
		return (w->builtin(d));
	else
		/* quotations not supported */
		return (d);
}

void
w_eval(struct w_env *e)
{
	struct w_node *n;
	struct w_word *w;

	while (e->code != NULL) {
		if (e->code->type == W_SYMBOL) {
			if ((w = w_lookup(e->dict, e->code->value.string)))
				e->data = w_call(w, e->data);
		} else {
			n	= w_copy_node(e->code);
			n->next	= e->data;
			e->data	= n;
		}

		e->code	= e->code->next;
	}
}

int
main(int argc, char *argv[])
{
	struct w_token	t;
	struct w_reader	r;
	struct w_env	e;
	struct w_node	*l;

	w_init_reader(&r, stdin);
	w_init_env(&e);

prompt:
	printf("OK ");

	while (1)
	{
		switch ((t = w_read_token(&r)).type)
		{
		case WT_EOF:
			return (0);
		case WT_EOL:
			w_eval(&e);
			goto prompt;
		case WT_COLON:
		case WT_SEMICOL:
		case WT_RSQUARE:
			break;
		default:
			l = w_extend(w_read_atom(&r, t), &e.code, &l);
		}
	}

	return (0);
}
