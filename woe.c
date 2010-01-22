#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W_SPACEP(x) (x == ' ' || x == '\t')

#define D1(e) e->data
#define D2(e) e->data->next
#define D3(e) e->data->next->next
#define D4(e) e->data->next->next->next

#define C1(e) e->code
#define C2(e) e->code->next

#define W_MAKE_NODE(x, typ, fld, val)					\
	struct w_node *x;						\
	x		= w_alloc_node();				\
	x->type		= typ;						\
	x->value.fld	= val;						\

#define W_ASSERT_ONE_ARG(e)						\
	if (D1(e) == NULL) {						\
		w_runtime_error(e, "stack underflow");			\
		return;							\
	}								\

#define W_ASSERT_TWO_ARGS(e)						\
	if (D1(e) == NULL || D2(e) == NULL) {				\
		w_runtime_error(e, "stack underflow");			\
		return;							\
	}								\

#define W_ASSERT_THREE_ARGS(e)						\
	if (D1(e) == NULL || D2(e) == NULL || D3(e) == NULL) {		\
		w_runtime_error(e, "stack underflow");			\
		return;							\
	}								\

#define W_ASSERT_ONE_TYPE(e, typ, msg)					\
	if (D1(e)->type != typ) {					\
		w_runtime_error(e, msg);				\
		return;							\
	}								\

#define W_ASSERT_TWO_TYPE(e, typ, msg)					\
	if (D1(e)->type != typ && D2(e)->type != typ) {			\
		w_runtime_error(e, msg);				\
		return;							\
	}								\

#define W_TYPE_PREDICATE(e, typ)					\
	W_MAKE_NODE(n, W_BOOL, fixnum, -1L);				\
	W_ASSERT_ONE_ARG(e);						\
	if (D1(e)->type == typ)						\
		n->value.fixnum = 0L;					\
	n->next	= D2(e);						\
	D1(e)	= n;							\

struct w_token {
	enum {
		WT_EOL,
		WT_EOF,

		WT_STRING,
		WT_FIXNUM,
		WT_FLONUM,
		WT_LSQUARE,
		WT_SYMBOL,

		WT_COLON,
		WT_SEMICOL,
		WT_RSQUARE
	} type;

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

struct w_node {
	enum {
		W_STRING,
		W_FIXNUM,
		W_FLONUM,
		W_BOOL,
		W_QUOT,
		W_SYMBOL
	} type;

	union {
		long	fixnum;
		double	flonum;
		char	*string;
		struct	w_node	*node;
	} value;

	struct	w_node	*next;
};

struct w_env {
	struct w_node *data;
	struct w_node *code;
	struct w_word *dict;
};

struct w_word {
	char		*name;
	void		(*builtin)(struct w_env*);
	struct w_node	*quot;
	struct w_word	*next;
};

struct w_builtin {
	char *name;
	void (*builtin)(struct w_env*);
};

static struct w_node*
w_alloc_node()
{
	return ((struct w_node*)malloc(sizeof(struct w_node)));
}

static struct w_word*
w_alloc_word()
{
	return ((struct w_word*)malloc(sizeof(struct w_word)));
}

static char*
w_alloc_string(size_t len)
{
	return ((char*)malloc(sizeof(char)*len));
}

static char*
w_copy_string(const char *o)
{
	char *s;

	s = w_alloc_string(strlen(o));
	strcpy(s, o);

	return (s);
}

static struct w_node*
w_copy_node(const struct w_node *o)
{
	if (o == NULL)
		return (NULL);
	{
		struct w_node *n, *q;

		n	= w_alloc_node();
		n->type	= o->type;

		switch (n->type) {
		case W_STRING:
			n->value.string	= w_copy_string(o->value.string);
			break;
		case W_QUOT:
		{
			struct w_node *t;

			n->value.node = q = w_copy_node(o->value.node);
			t = o->value.node;
			while (t != NULL) {
				q->next = w_copy_node(t->next);
				q	= q->next;
				t	= t->next;
			}
			break;
		}
		default:
			n->value = o->value;
		}

		return (n);
	}
}

static void w_pn(const struct w_node *n);

static void
w_p(const struct w_node *n)
{
	if (n == NULL)
		return;

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
	case W_BOOL:
		if (n->value.fixnum == 0L)
			printf("true ");
		else
			printf("false ");
		break;
	case W_QUOT:
		printf("[ ");
		w_pn(n->value.node);
		printf("] ");
		break;
	case W_SYMBOL:
		printf("%s ", n->value.string);
		break;
	}
}

static void
w_pn(const struct w_node *n)
{
	while (n != NULL) {
		w_p(n);
		n = n->next;
	}
}

static void
w_init_reader(struct w_reader *r, FILE *f)
{
	r->stream	= f;
	r->line		= 1;
	r->column	= 0;
	r->buflen	= 0;
	r->bufpos	= 0;
}

static char
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

static void
w_unread_char(struct w_reader *r)
{
	r->bufpos--;
	r->column--;
}

static struct w_token
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

static struct w_token
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

static struct w_token
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
		if (W_SPACEP(c) || c == '\n' || strchr("[]:;", c)) {
			w_unread_char(r);
			break;
		}
		buffer[pos++] = c;
	}

	buffer[pos++] = '\0';
	t.value.string = w_alloc_string(pos);
	strncpy(t.value.string, buffer, pos);

	return (t);
}

static struct w_token
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

static void
w_read_error(const char *msg)
{
	printf("READ ERROR: %s\n", msg);
}

static void
w_runtime_error(struct w_env *e, const char *msg)
{
	printf("ERROR: %s\n", msg);
	C1(e) = NULL;
}

static struct w_node*
w_make_fixnum(long value)
{
	W_MAKE_NODE(n, W_FIXNUM, fixnum, value);

	return (n);
}

static struct w_node*
w_make_flonum(double value)
{
	W_MAKE_NODE(n, W_FLONUM, flonum, value);

	return (n);
}

static struct w_node*
w_make_bool(long value)
{
	W_MAKE_NODE(n, W_BOOL, fixnum, value);

	return (n);
}

static struct w_node*
w_make_string(char *value)
{
	W_MAKE_NODE(n, W_STRING, string, value);

	return (n);
}

static struct w_node*
w_make_symbol(char *value)
{
	W_MAKE_NODE(n, W_SYMBOL, string, value);

	return (n);
}

static struct w_node*
w_make_quot()
{
	W_MAKE_NODE(n, W_QUOT, node, NULL);

	return (n);
}

static struct w_node*
w_extend(struct w_node *n, struct w_node **h, struct w_node **l)
{
	if (*h == NULL)
		*h = n;
	if (*l == NULL)
		*l = n;
	else
		(*l)->next = n;

	return (n);
}

static struct w_node* w_read_quot(struct w_reader*);

static struct w_node*
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

static struct w_node*
w_read_quot(struct w_reader *r)
{
	struct w_token	t;
	struct w_node	*l, *n;

	n = w_make_quot();
	l = NULL;

	while ((t = w_read_token(r)).type <= WT_SYMBOL) {
		if (t.type == WT_EOF) {
			w_read_error("unexpected end-of-file");

			return (NULL);
		}
		if (t.type != WT_EOL)
			l = w_extend(w_read_atom(r, t), &n->value.node, &l);
	}

	return (n);
}

static struct w_word*
w_read_def(struct w_reader *r)
{
	struct w_token	t;
	struct w_word	*w;

	w = w_alloc_word();

	if ((t = w_read_token(r)).type == WT_SYMBOL) {
		w->name	= t.value.string;
		w->quot	= w_read_quot(r);

		return (w);
	}

	w_read_error("expected word name after ':'");

	return (NULL);
}

static struct w_word*
w_lookup(struct w_word *w, const char *name)
{
	while (w != NULL) {
		if (strcasecmp(w->name, name) == 0)
			return (w);
		w = w->next;
	}

	return (NULL);
}

static void w_eval(struct w_env*);

static void
w_eval_quot(struct w_env* e, struct w_node* n)
{
	struct w_env i;

	i.data = D1(e);
	i.dict = e->dict;
	i.code = n->value.node;

	w_eval(&i);

	D1(e)	= i.data;
}

static void
w_call(const struct w_word *w, struct w_env *e)
{
	if (w->builtin != NULL)
		w->builtin(e);
	else
		w_eval_quot(e, w->quot);
}

static void
w_eval(struct w_env *e)
{
	while (C1(e) != NULL) {
		if (C1(e)->type == W_SYMBOL) {
			struct w_word *w;

			if ((w = w_lookup(e->dict, C1(e)->value.string))) {
				C1(e) = C2(e);
				w_call(w, e);
			} else {
				w_runtime_error(e, "undefined word");
			}
		} else {
			struct w_node *n;

			n	= w_copy_node(C1(e));
			n->next	= D1(e);
			D1(e)	= n;
			C1(e)	= C2(e);
		}
	}
}

static void
w_swap(struct w_env *e)
/* (b a -- a b) */
{
	struct w_node *n;

	W_ASSERT_TWO_ARGS(e);

	n	= D2(e);
	D2(e)	= n->next;
	n->next	= D1(e);
	D1(e)	= n;
}

static void
w_dup(struct w_env *e)
/* (a -- a a) */
{
	struct w_node *n;

	W_ASSERT_ONE_ARG(e);

	n	= w_copy_node(D1(e));
	n->next	= D1(e);
	D1(e)	= n;
}

static void
w_pop(struct w_env *e)
/* (b a -- b) */
{
	W_ASSERT_ONE_ARG(e);

	D1(e) = D2(e);
}

static void
w_cat(struct w_env *e)
/* ([b] [a] -- [b a]) */
{
	struct w_node *l;

	W_ASSERT_TWO_ARGS(e);
	W_ASSERT_TWO_TYPE(e, W_QUOT, "cannot concatenate non-quotations");

	l = D2(e)->value.node;

	if (l != NULL) {
		while (l->next != NULL)
			l = l->next;

		l->next = D1(e)->value.node;
	}

	w_pop(e);
}

static void
w_cons(struct w_env *e)
/* (b [a] -- [b a]) */
{
	struct w_node *b;

	W_ASSERT_TWO_ARGS(e);
	W_ASSERT_ONE_TYPE(e, W_QUOT, "cannot cons onto a non-quotation");

	b			= D2(e);
	D2(e)			= D3(e);
	b->next			= D1(e)->value.node;
	D1(e)->value.node	= b;
}

static void
w_i(struct w_env *e)
/* ([a] -- ) */
{
	struct w_node *n;

	W_ASSERT_ONE_ARG(e);
	W_ASSERT_ONE_TYPE(e, W_QUOT, "cannot evaluate a non-quotation");

	n	= D1(e);
	D1(e)	= D2(e);

	w_eval_quot(e, n);
}

static void
w_true(struct w_env *e)
/* ( -- ?) */
{
	struct w_node *n;

	n = w_make_bool(0L);

	n->next	= D1(e);
	D1(e)	= n;
}

static void
w_false(struct w_env *e)
/* ( -- ?) */
{
	struct w_node *n;

	n = w_make_bool(-1L);

	n->next	= D1(e);
	D1(e)	= n;
}

static void
w_fixnump(struct w_env *e)
/* (a -- ?) */
{
	W_TYPE_PREDICATE(e, W_FIXNUM);
}

static void
w_flonump(struct w_env *e)
/* (a -- ?) */
{
	W_TYPE_PREDICATE(e, W_FLONUM);
}

static void
w_boolp(struct w_env *e)
/* (a -- ?) */
{
	W_TYPE_PREDICATE(e, W_BOOL);
}

static void
w_stringp(struct w_env *e)
/* (a -- ?) */
{
	W_TYPE_PREDICATE(e, W_STRING);
}

static void
w_quotationp(struct w_env *e)
/* (a -- ?) */
{
	W_TYPE_PREDICATE(e, W_QUOT);
}

static void
w_branch(struct w_env *e)
/* (? [t] [f] -- ) */
{
	struct w_node *n;

	W_ASSERT_THREE_ARGS(e);
	W_ASSERT_TWO_TYPE(e, W_QUOT, "cannot branch to a non-quotation");

	if (D3(e)->value.fixnum == 0L)
		n = D2(e);
	else
		n = D1(e);

	D1(e) = D4(e);

	w_eval_quot(e, n);
}

static void
w_print(struct w_env *e)
/* (a -- a) */
{
	w_p(D1(e));
}

struct w_builtin initial_dict[] = {
	{ "SWAP",	w_swap		},
	{ "DUP",	w_dup		},
	{ "POP",	w_pop		},
	{ "CAT",	w_cat		},
	{ "CONS",	w_cons		},
	{ "I",		w_i		},
	{ "TRUE",	w_true		},
	{ "FALSE",	w_false		},
	{ "FIXNUM?",	w_fixnump	},
	{ "FLONUM?",	w_flonump	},
	{ "BOOLEAN?",	w_boolp		},
	{ "STRING?",	w_stringp	},
	{ "QUOTATION?",	w_quotationp	},
	{ "BRANCH",	w_branch	},
	{ "PRINT",	w_print		}
};

static struct w_word*
w_make_builtin_dict()
{
	int			i, len;
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

static void
w_load(struct w_env *e, FILE *f, const char *prompt)
{
	struct w_token	t;
	struct w_reader	r;
	struct w_node	*l;

	w_init_reader(&r, f);

	l = NULL;
prompt:
	if (prompt != NULL)
		printf("%s ", prompt);

	while (1)
	{
		switch ((t = w_read_token(&r)).type)
		{
		case WT_EOF:
			fclose(f);
			return;
		case WT_EOL:
			w_eval(e);
			goto prompt;
		case WT_COLON:
		{
			struct w_word *w;

			if ((w = w_read_def(&r)) != NULL) {
				w->next	= e->dict;
				e->dict	= w;
			}
			break;
		}
		case WT_SEMICOL:
		case WT_RSQUARE:
			break;
		default:
			l = w_extend(w_read_atom(&r, t), &e->code, &l);
		}
	}
}

static void
w_init_env(struct w_env *e)
{
	e->data	= NULL;
	e->code = NULL;
	e->dict = w_make_builtin_dict();
}

int
main(int argc, char *argv[])
{
	struct w_env e;

	w_init_env(&e);

	if (argc == 2) {
		FILE *f;
		if ((f = fopen(argv[1], "r")) != NULL)
			w_load(&e, f, NULL);
		else
			perror(argv[1]);
	}

	w_load(&e, stdin, "W>");

	return (0);
}
