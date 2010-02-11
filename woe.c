#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W_HEAP_SIZE	1024*512
#define W_TRUE		1
#define W_FALSE		0

#define W_SPACEP(x) (x == ' ' || x == '\t')

#define D1(e) e->data
#define D2(e) e->data->next
#define D3(e) e->data->next->next
#define D4(e) e->data->next->next->next

#define C1(e) e->code
#define C2(e) e->code->next

#define W_MAKE_NODE(e, x, typ, fld, val)				\
	do {								\
		x		= w_alloc_node(e->heap);		\
		x->type		= typ;					\
		x->value.fld	= val;					\
		x->to		= NULL;					\
		x->next		= NULL;					\
	} while (0);							\

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
	do {								\
		struct w_node *_n;					\
		W_MAKE_NODE(e, _n, W_BOOL, fixnum, W_FALSE);		\
		W_ASSERT_ONE_ARG(e);					\
		if (D1(e)->type == typ)					\
			_n->value.fixnum = W_TRUE;			\
		_n->next	= D2(e);				\
		D1(e)		= _n;					\
	} while (0);							\

#define W_COMPOSITEP(n) (n->type == W_STRING || n->type == W_SYMBOL)

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

struct w_env {
	struct w_node *data;
	struct w_node *code;
	struct w_node *dict;

	struct w_heap *heap;
};

struct w_node {
	enum {
		/* immediate types	*/
		W_FIXNUM,
		W_FLONUM,
		W_BOOL,

		/* composite types	*/
		W_STRING,
		W_SYMBOL,

		/* nested types		*/
		W_QUOT
	} type;

	union {
		long	fixnum;
		double	flonum;

		struct {
			size_t	length;
			char	*bytes;
		} data;

		struct {
			struct	w_node *name;
			struct 	w_node *quot;
			void	(*func)(struct w_env*);
		} code;

	} value;

	struct w_node *to;
	struct w_node *next;
};

struct w_heap {
	char	*data;
	size_t	used;
	size_t	size;
};

struct w_builtin {
	char *name;
	void (*builtin)(struct w_env*);
};

static void*
w_alloc(struct w_heap *h, size_t n)
{
	void *p;

	p	= h->data + h->used;
	h->used	+= n;

	return (p);
}

static struct w_node*
w_alloc_node(struct w_heap *h)
{
	return ((struct w_node*)w_alloc(h, sizeof(struct w_node)));
}

static char*
w_alloc_string(struct w_heap *h, size_t len)
{
	return ((char*)w_alloc(h, sizeof(char)*len));
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
		printf("\"%s\" ", n->value.data.bytes);
		break;
	case W_FIXNUM:
		printf("%ld ", n->value.fixnum);
		break;
	case W_FLONUM:
		printf("%.2f ", n->value.flonum);
		break;
	case W_BOOL:
		if (n->value.fixnum)
			printf("true ");
		else
			printf("false ");
		break;
	case W_QUOT:
		printf("[ ");
		if (n->value.code.func != NULL)
			printf("<func>");
		else
			w_pn(n->value.code.quot);
		printf("] ");
		break;
	case W_SYMBOL:
		printf("%s ", n->value.data.bytes);
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

	if (r->bufpos == r->buflen) {
		if (fgets(r->buffer, 1024, r->stream) == NULL)
			return ('\0');
		r->buflen = strlen(r->buffer);
		r->bufpos = 0;
	}

	c = r->buffer[r->bufpos++];

	if (c == '\n') {
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
w_read_string(struct w_env *e, struct w_reader *r)
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
	t.value.string = w_alloc_string(e->heap, pos);
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

	while ((c = w_read_char(r)) != '\0') {
		if (!strchr("0123456789+-", c)) {
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
w_read_symbol(struct w_env *e, struct w_reader *r)
{
	size_t	pos;
	char	c;
	char	buffer[1024];
	struct	w_token t;

	pos	= 0;
	t.type	= WT_SYMBOL;

	while ((c = w_read_char(r)) != '\0') {
		if (W_SPACEP(c) || c == '\n' || strchr("[]:;", c)) {
			w_unread_char(r);
			break;
		}
		buffer[pos++] = c;
	}

	buffer[pos++] = '\0';
	t.value.string = w_alloc_string(e->heap, pos);
	strncpy(t.value.string, buffer, pos);

	return (t);
}

static struct w_token
w_read_token(struct w_env *e, struct w_reader *r)
{
	char	c;
	struct	w_token t;

restart:
	do {
		c = w_read_char(r);
	} while (W_SPACEP(c));

	if (c == '(') {
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
		return (w_read_string(e, r));
	case '-':
		w_unread_char(r);
		if ((t = w_read_number(r)).type != WT_SYMBOL)
			return (t);
	default:
		w_unread_char(r);
		if (isdigit(c))
			t = w_read_number(r);
		else
			t = w_read_symbol(e, r);
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
w_make_fixnum(struct w_env *e, long value)
{
	struct w_node *n;

	W_MAKE_NODE(e, n, W_FIXNUM, fixnum, value);

	return (n);
}

static struct w_node*
w_make_flonum(struct w_env *e, double value)
{
	struct w_node *n;

	W_MAKE_NODE(e, n, W_FLONUM, flonum, value);

	return (n);
}

static struct w_node*
w_make_bool(struct w_env *e, long value)
{
	struct w_node *n;

	W_MAKE_NODE(e, n, W_BOOL, fixnum, value);

	return (n);
}

static struct w_node*
w_make_string(struct w_env *e, char *value)
{
	struct w_node *n;

	W_MAKE_NODE(e, n, W_STRING, data.bytes, value);
	n->value.data.length = strlen(value) + 1;

	return (n);
}

static struct w_node*
w_make_symbol(struct w_env *e, char *value)
{
	struct w_node *n;

	W_MAKE_NODE(e, n, W_SYMBOL, data.bytes, value);
	n->value.data.length = strlen(value) + 1;

	return (n);
}

static struct w_node*
w_make_quot(struct w_env *e)
{
	struct w_node *n;

	W_MAKE_NODE(e, n, W_QUOT, code.quot, NULL);
	n->value.code.name = NULL;
	n->value.code.func = NULL;

	return (n);
}

char*
w_copy_string(struct w_env *e, const char *o)
{
	char *s;

	s = w_alloc_string(e->heap, strlen(o) + 1);
	strcpy(s, o);

	return (s);
}

static struct w_node*
w_copy_node(struct w_env *e, const struct w_node *o)
{
	if (o == NULL)
		return (NULL);
	{
		struct w_node *n, *q;

		switch (o->type) {
		case W_STRING:
		case W_SYMBOL: {
			W_MAKE_NODE(e, n, o->type, data.bytes, NULL);
			n->value.data.bytes =
				w_copy_string(e, o->value.data.bytes);
			n->value.data.length = o->value.data.length;
			return (n);
		}
		case W_QUOT: {
			struct w_node *t;

			n = w_make_quot(e);

			if (o->value.code.func != NULL) {
				n->value.code.func =
					o->value.code.func;
				n->value.code.name =
					w_copy_node(e, o->value.code.name);

				return (n);
			}

			n->value.code.quot = q =
				w_copy_node(e, o->value.code.quot);
			t = o->value.code.quot;

			while (t != NULL) {
				q->next = w_copy_node(e, t->next);
				q	= q->next;
				t	= t->next;
			}

			return (n);
		}
		default:
			W_MAKE_NODE(e, n, o->type, fixnum, 0);
			n->value = o->value;
		}

		return (n);
	}
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

static struct w_node* w_read_quot(struct w_env *e, struct w_reader*);

static struct w_node*
w_read_atom(struct w_env *e, struct w_reader *r, struct w_token t)
{
	switch (t.type)
	{
	case WT_FIXNUM:
		return (w_make_fixnum(e, t.value.fixnum));
	case WT_FLONUM:
		return (w_make_flonum(e, t.value.flonum));
	case WT_STRING:
		return (w_make_string(e, t.value.string));
	case WT_SYMBOL:
		return (w_make_symbol(e, t.value.string));
	case WT_LSQUARE:
		return (w_read_quot(e, r));
	default:
		return (NULL);
	}
}

static struct w_node*
w_read_quot(struct w_env *e, struct w_reader *r)
{
	struct w_token	t;
	struct w_node	*l, *n;

	n = w_make_quot(e);
	l = NULL;

	while ((t = w_read_token(e, r)).type <= WT_SYMBOL) {
		if (t.type == WT_EOF) {
			w_read_error("unexpected end-of-file");

			return (NULL);
		}
		if (t.type != WT_EOL)
			l = w_extend(w_read_atom(e, r, t),
				     &n->value.code.quot, &l);
	}

	return (n);
}

static struct w_node*
w_read_def(struct w_env *e, struct w_reader *r)
{
	struct w_token	t;
	struct w_node	*w;

	W_MAKE_NODE(e, w, W_QUOT, code.quot, NULL);

	if ((t = w_read_token(e, r)).type == WT_SYMBOL) {
		w->value.code.name = w_make_string(e, t.value.string);
		w->value.code.quot = w_read_quot(e, r);

		return (w);
	}

	w_read_error("expected word name after ':'");

	return (NULL);
}

static struct w_heap*
w_make_heap(size_t size)
{
	struct w_heap *h;

	h 	= (struct w_heap*)malloc(sizeof(struct w_heap));
	h->used	= 0;
	h->size	= size;
	h->data	= (char*)malloc(size);

	if (h == NULL || h->data == NULL) {
		perror("ERROR:");
		exit (1);
	}

	return(h);
}

size_t
w_node_length(struct w_node *n)
{
	if (n != NULL && W_COMPOSITEP(n)) {
		return (sizeof(struct w_node) + n->value.data.length);
	}

	return (sizeof(struct w_node));
}

static void
w_copy(struct w_node **p, struct w_heap *h)
{
	if (*p == NULL)
		return;

	if ((*p)->to == NULL) {
		struct w_node *t;
		t		= w_alloc_node(h);
		memcpy(t, *p, sizeof(struct w_node));
		(*p)->to	= t;
		*p		= t;

		if (W_COMPOSITEP((*p))) {
			void	*c;
			size_t	l;

			l	= t->value.data.length;
			c	= w_alloc(h, l);
			memcpy(c, t->value.data.bytes, l);
			t->value.data.bytes = (char*)c;
		}
	} else
		*p = (*p)->to;
}

static void
w_gc(struct w_env *e)
{
	char		*r;
	struct w_node	*t;
	struct w_heap	*h;

	h = w_make_heap(e->heap->size);
	r = h->data;

	w_copy(&e->data, h);
	w_copy(&e->code, h);
	w_copy(&e->dict, h);

	while (r < (h->data + h->used)) {
		t = (struct w_node*)r;

		if (t->type == W_QUOT) {
			if (t->value.code.func == NULL) {
				w_copy(&t->value.code.quot, h);
			}

			w_copy(&t->value.code.name, h);
		}

		w_copy(&t->next, h);
		r += w_node_length(t);
	}

	free(e->heap->data);
	free(e->heap);
	e->heap = h;
}

static struct w_node*
w_lookup(struct w_node *w, const char *name)
{
	char *s;

	while (w != NULL) {
		s = w->value.code.name->value.data.bytes;
		if (strcasecmp(s, name) == 0)
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
	i.heap = e->heap;
	i.code = n->value.code.quot;

	w_eval(&i);

	D1(e)	= i.data;
}

static void
w_call(const struct w_node *w, struct w_env *e)
{
	if (w->value.code.func != NULL)
		w->value.code.func(e);
	else
		w_eval_quot(e, w->value.code.quot);
}

static void
w_eval(struct w_env *e)
{
	while (C1(e) != NULL) {
		if (C1(e)->type == W_SYMBOL) {
			struct w_node *w;

			if ((w = w_lookup(e->dict, C1(e)->value.data.bytes))) {
				C1(e) = C2(e);
				w_call(w, e);
			} else {
				w_runtime_error(e, "undefined word");
			}
		} else {
			struct w_node *n;

			n	= w_copy_node(e, C1(e));
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

	n	= w_copy_node(e, D1(e));
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

	l = D2(e)->value.code.quot;

	if (l != NULL) {
		while (l->next != NULL)
			l = l->next;

		l->next = D1(e)->value.code.quot;
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
	b->next			= D1(e)->value.code.quot;
	D1(e)->value.code.quot	= b;
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

	n = w_make_bool(e, W_TRUE);

	n->next	= D1(e);
	D1(e)	= n;
}

static void
w_false(struct w_env *e)
/* ( -- ?) */
{
	struct w_node *n;

	n = w_make_bool(e, W_FALSE);

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

	if (D3(e)->value.fixnum)
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

static struct w_node*
w_make_builtin_dict(struct w_env *e)
{
	int			i, len;
	struct w_node		*p;
	struct w_node		*w;
	struct w_builtin	*d;

	len	= sizeof(initial_dict) / sizeof(initial_dict[0]);
	d	= initial_dict;
	p	= NULL;

	for (i = 0; i < len; i++) {
		w			= w_make_quot(e);
		w->value.code.name	= w_make_string(e, d[i].name);
		w->value.code.func	= d[i].builtin;
		w->next			= p;
		p			= w;
	}

	return (w);
}

static void
w_load(struct w_env *e, FILE *f, char prompt)
{
	struct w_token	t;
	struct w_reader	r;
	struct w_node	*l;

	w_init_reader(&r, f);

	l = NULL;
prompt:
	w_gc(e);

	if (prompt)
		printf("(USED: %dB) ", (int)e->heap->used);

	while (1)
	{
		switch ((t = w_read_token(e, &r)).type)
		{
		case WT_EOF:
			fclose(f);
			return;
		case WT_EOL:
			w_eval(e);
			goto prompt;
		case WT_COLON:
		{
			struct w_node *w;

			if ((w = w_read_def(e, &r)) != NULL) {
				w->next	= e->dict;
				e->dict	= w;
			}
			break;
		}
		case WT_SEMICOL:
		case WT_RSQUARE:
			break;
		default:
			l = w_extend(w_read_atom(e, &r, t), &e->code, &l);
		}
	}
}

static void
w_init_env(struct w_env *e, struct w_heap *h)
{
	e->data	= NULL;
	e->code = NULL;
	e->heap = h;
	e->dict = w_make_builtin_dict(e);
}

int
main(int argc, char *argv[])
{
	struct w_env	e;
	struct w_heap	*h;

	h = w_make_heap(W_HEAP_SIZE);

	w_init_env(&e, h);

	if (argc == 2) {
		FILE *f;
		if ((f = fopen(argv[1], "r")) != NULL)
			w_load(&e, f, W_FALSE);
		else
			perror(argv[1]);
	}

	w_load(&e, stdin, W_TRUE);

	return (0);
}
