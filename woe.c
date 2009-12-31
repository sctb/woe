#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define w_whitespacep(x) (x == ' ' || x == '\t')

enum w_token_type {
	/* factors */
	WT_STRING,
	WT_FIXNUM,
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
	char* string;
};

struct w_reader {
	FILE *stream;
	int line;
	int column;
	int len;
	char buffer[1024];
};

enum w_tag {
	W_BOOL,
	W_STRING,
	W_FIXNUM,
	W_QUOT
};

union w_value {
	long fixnum;
	char *string;
	void(*proc)();
	struct w_node *node;
	struct w_entry *entry;
};

struct w_node {
	enum w_tag tag;
	union w_value value;
	struct w_node *next;
};

struct w_entry {
	char *name;
	union {
		struct w_node *body;
		void(*proc)();
	} value;
	struct w_entry *next;
};

void
w_init_reader(struct w_reader *r, FILE *stream)
{
	r->stream = stream;
	r->line = 0;
	r->column = 0;
	r->len = 0;
}

char
w_read_char(struct w_reader *r)
{
	if(r->column == r->len)
	{
		if(fgets(r->buffer, 1024, r->stream) == NULL)
			return '\0';

		r->line++;
		r->column = 0;
		r->len = strlen(r->buffer);
	}

	return r->buffer[r->column++];
}

struct w_token
w_read_token(struct w_reader *r)
{
	int pos;
	char c;
	char buffer[1024];
	struct w_token token;

	pos = 0;

restart:
	do {
		c = w_read_char(r);
	} while(w_whitespacep(c));

	if(c == '(')
	{
		do {
			c = w_read_char(r);
		} while(c != ')');
		goto restart;
	}

	switch(c)
	{
	case '\n':
		token.type = WT_EOL;
		return token;
	case '\0':
		token.type = WT_EOF;
		return token;
	case '[':
		token.type = WT_LSQUARE;
		return token;
	case ']':
		token.type = WT_RSQUARE;
		return token;
	case ':':
		token.type = WT_COLON;
		return token;
	case ';':
		token.type = WT_SEMICOL;
		return token;
	case '"':
		while ((c = w_read_char(r)) != '"') {
			if(c == '\\') {
				buffer[pos++] = c;
				c = w_read_char(r);
			}
			buffer[pos++] = c;
		}
		buffer[pos] = '\0';
		token.type = WT_STRING;
		token.string = (char*)malloc(strlen(buffer) + 1);
		strcpy(token.string, buffer);
		return token;
	}
}

int
main(int argc, char *argv[])
{
	struct w_token t;
	struct w_reader r;

	w_init_reader(&r, stdin);

	while(1)
	{
		printf("OK ");

		while((t = w_read_token(&r)).type != WT_EOL)
		{
			if(t.type == WT_EOF)
				return 0;

			switch(t.type)
			{
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
				printf("STRING: %s\n", t.string);
				free(t.string);
				break;
			}
		}
	}

	return 0;
}
