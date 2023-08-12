// SPDX-License-Identifier: GPL-2.0-only
/*
 * Helper to find identifier names in C files
 *
 * Copyright (C) Johan Malm 2023
 *
 * It tokenizes the specified C file and searches all identifier-tokens against
 * the specified patterns.
 *
 * An identifier in this context is any alphanumeric/underscore string starting
 * with a letter [A-Za-z] or underscore. It represents entities such as
 * functions, variables, user-defined data types and C language keywords.
 * Alphanumeric strings within comments are ignored, but not parsing of tokens
 * is carried out to understand their semantic meaning.
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct buf {
	char *buf;
	int alloc;
	int len;
};

enum token_kind {
	TOKEN_NONE = 0,
	TOKEN_IDENTIFIER, /* For example: static extern if while */
	TOKEN_LITERAL, /* For example: 0xff 42 "foo" */
	TOKEN_SPECIAL, /* For example: ++ -= ! ... */
};

struct token {
	int line;
	enum token_kind kind;
	struct buf name;
	unsigned int special;
};

enum {
	SPECIAL_ELLIPSIS = 256,
	SPECIAL_ASSIGN,
	SPECIAL_BIT_OP,
	SPECIAL_INC_OP,
	SPECIAL_DEC_OP,
	SPECIAL_PTR_OP,
	SPECIAL_AND_OP,
	SPECIAL_OR_OP,
	SPECIAL_COMPARISON_OP,
	SPECIAL_COMMENT_BEGIN,
	SPECIAL_COMMENT_END,
	SPECIAL_COMMENT_LINE_BEGIN,
};

static char *current_buffer_position;
static struct token *tokens;
static int nr_tokens, alloc_tokens;
static int current_line;
static char **argv_tokens;
static int found_token;

static const char find_banned_usage[] =
"Usage: find-banned [OPTIONS...] FILE\n"
"When FILE is -, read stdin\n"
"OPTIONS:\n"
"  --tokens=<tokens>     Comma-separated string of idents to grep for\n";

static void
usage(void)
{
	printf("%s", find_banned_usage);
	exit(0);
}

char **
split(char *str, char delim)
{
	if (!str) {
		return NULL;
	}
	int argc = 1;
	char *p = str;
	while (*p) {
		if (*p == delim) {
			argc++;
		}
		++p;
	}

	char **argv = calloc(argc + 1, sizeof(*argv));
	char **argvp = argv;
	p = str;
	while (*str) {
		if (*str == delim) {
			*argvp++ = strndup(p, str-p);
			p = str + 1;
		}
		++str;
	}
	*argvp++ = strndup(p, str-p);
	*argvp = NULL;
	return argv;
}

void
buf_init(struct buf *s)
{
	s->alloc = 256;
	s->buf = malloc(s->alloc);
	s->buf[0] = '\0';
	s->len = 0;
}

void
buf_add(struct buf *s, const char *data, size_t len)
{
	if (!data || data[0] == '\0') {
		return;
	}
	if (s->alloc <= s->len + len + 1) {
		s->alloc = s->alloc + len;
		s->buf = realloc(s->buf, s->alloc);
	}
	memcpy(s->buf + s->len, data, len);
	s->len += len;
	s->buf[s->len] = 0;
}

void
buf_add_char(struct buf *s, char ch)
{
	if (s->alloc <= s->len + 1) {
		s->alloc = s->alloc * 2 + 16;
		s->buf = realloc(s->buf, s->alloc);
	}
	s->buf[s->len++] = ch;
	s->buf[s->len] = 0;
}

static struct token *
add_token(void)
{
	if (nr_tokens == alloc_tokens) {
		alloc_tokens = (alloc_tokens + 16) * 2;
		tokens = realloc(tokens, alloc_tokens * sizeof(struct token));
	}
	struct token *token = tokens + nr_tokens;
	memset(token, 0, sizeof(*token));
	nr_tokens++;
	buf_init(&token->name);
	token->line = current_line;
	return token;
}

static void
get_identifier_token(struct token *token)
{
	buf_add_char(&token->name, current_buffer_position[0]);
	current_buffer_position++;
	if (isspace(current_buffer_position[0])) {
		return;
	}
	switch (current_buffer_position[0]) {
	case '\0':
		break;
	case 'a' ... 'z':
	case 'A' ... 'Z':
	case '0' ... '9':
	case '_':
	case '#':
		get_identifier_token(token);
		break;
	default:
		break;
	}
}

static void
get_number_token(struct token *token)
{
	buf_add_char(&token->name, current_buffer_position[0]);
	current_buffer_position++;
	if (isspace(current_buffer_position[0])) {
		return;
	}
	switch (current_buffer_position[0]) {
	case '\0':
		break;
	case '0' ... '9':
	case 'a' ... 'f':
	case 'A' ... 'F':
	case 'x':
		get_number_token(token);
		break;
	default:
		break;
	}
}

struct {
	const char *combo;
	unsigned int special;
} specials[] = {
	{ "...", SPECIAL_ELLIPSIS },
	{ ">>=", SPECIAL_ASSIGN },
	{ "<<=", SPECIAL_ASSIGN },
	{ "+=", SPECIAL_ASSIGN },
	{ "-=", SPECIAL_ASSIGN },
	{ "*=", SPECIAL_ASSIGN },
	{ "/=", SPECIAL_ASSIGN },
	{ "%=", SPECIAL_ASSIGN },
	{ "&=", SPECIAL_ASSIGN },
	{ "^=", SPECIAL_ASSIGN },
	{ "|=", SPECIAL_ASSIGN },
	{ ">>", SPECIAL_BIT_OP },
	{ "<<", SPECIAL_BIT_OP },
	{ "++", SPECIAL_INC_OP },
	{ "--", SPECIAL_DEC_OP },
	{ "->", SPECIAL_PTR_OP },
	{ "&&", SPECIAL_AND_OP },
	{ "||", SPECIAL_OR_OP },
	{ "<=", SPECIAL_COMPARISON_OP },
	{ ">=", SPECIAL_COMPARISON_OP },
	{ "==", SPECIAL_COMPARISON_OP },
	{ "!=", SPECIAL_COMPARISON_OP },
	{ "/*", SPECIAL_COMMENT_BEGIN },
	{ "*/", SPECIAL_COMMENT_END },
	{ "//", SPECIAL_COMMENT_LINE_BEGIN },
	{ ";", ';' },
	{ "{", '{' },
	{ "}", '}' },
	{ ",", ',' },
	{ ":", ':' },
	{ "=", '=' },
	{ "(", '(' },
	{ ")", ')' },
	{ "[", '[' },
	{ "]", ']' },
	{ ".", '.' },
	{ "&", '&' },
	{ "!", '!' },
	{ "~", '~' },
	{ "-", '-' },
	{ "+", '+' },
	{ "*", '*' },
	{ "/", '/' },
	{ "%", '%' },
	{ "<", '<' },
	{ ">", '>' },
	{ "^", '^' },
	{ "|", '|' },
	{ "?", '?' },
};

static void
get_special_token(struct token *token)
{
#define MAX_SPECIAL_LEN (3)
	/* Peek up to MAX_SPECIAL_LEN-1 characters ahead */
	char buf[MAX_SPECIAL_LEN + 1] = { 0 };
	for (int i = 0; i < MAX_SPECIAL_LEN; i++) {
		buf[i] = current_buffer_position[i];
		if (!current_buffer_position[i]) {
			break;
		}
	}
#undef MAX_SPECIAL_LEN

	/* Compare with longest special tokens first */
	int k;
	for (k = strlen(buf); k > 0; k--) {
		for (size_t j = 0; j < sizeof(specials) / sizeof(specials[0]); j++) {
			if (strlen(specials[j].combo) < k) {
				break;
			}
			if (!strcmp(specials[j].combo, buf)) {
				buf_add(&token->name, buf, k);
				token->special = specials[j].special;
				goto done;
			}
		}
		buf[k - 1] = '\0';
	}
done:
	current_buffer_position += token->name.len;
}

static void
handle_preprocessor_directive(void)
{
	/* We just ignore preprocessor lines */
	for (;;) {
		++current_buffer_position;
		if (current_buffer_position[0] == '\0') {
			return;
		}
		if (current_buffer_position[0] == '\n') {
			++current_line;
			return;
		}
	}
}

struct token *
lex(char *buffer)
{
	tokens = NULL;
	nr_tokens = 0;
	alloc_tokens = 0;
	bool in_single_comment = false;

	current_buffer_position = buffer;

	for (;;) {
		struct token *token = NULL;
		switch (current_buffer_position[0]) {
		case '\0':
			goto out;
		case 'a' ... 'z':
		case 'A' ... 'Z':
		case '_':
			token = add_token();
			get_identifier_token(token);
			token->kind = TOKEN_IDENTIFIER;
			continue;
		case '0' ... '9':
			token = add_token();
			get_number_token(token);
			token->kind = TOKEN_LITERAL;
			continue;
		case '+': case '-': case '*': case '/': case '%': case '.':
		case '>': case '<': case '=': case '!': case '&': case '|':
		case '^': case '{': case '}': case '(': case ')': case ',':
		case ';': case ':': case '[': case ']': case '~': case '?':
			token = add_token();
			get_special_token(token);
			token->kind = TOKEN_SPECIAL;
			if (token->special == SPECIAL_COMMENT_LINE_BEGIN) {
				token->special = SPECIAL_COMMENT_BEGIN;
				in_single_comment = true;
			}
			continue;
		case '#':
			handle_preprocessor_directive();
			break;
		case '\n':
			if (in_single_comment) {
				token = add_token();
				token->kind = TOKEN_SPECIAL;
				token->special = SPECIAL_COMMENT_END;
				in_single_comment = false;
			}
			++current_line;
			break;
		default:
			break;
		}
		++current_buffer_position;
	}
out:
	add_token(); /* end marker */
	return tokens;
}

char *
read_file(const char *filename)
{
	char *line = NULL;
	size_t len = 0;
	FILE *stream = fopen(filename, "r");
	if (!stream) {
		fprintf(stderr, "warn: cannot read '%s'\n", filename);
		return NULL;
	}
	struct buf buffer;
	buf_init(&buffer);
	while ((getline(&line, &len, stream) != -1)) {
		buf_add(&buffer, line, strlen(line));
	}
	free(line);
	fclose(stream);
	return buffer.buf;
}

static bool
grep(struct token *tokens, const char *filename, const char *pattern)
{
	bool found = false;
	unsigned int in_comment = 0;

	for (struct token *t = tokens; t->kind; t++) {
		if (t->kind == TOKEN_SPECIAL) {
			if (t->special == SPECIAL_COMMENT_BEGIN) {
				++in_comment;
			} else if (t->special == SPECIAL_COMMENT_END) {
				--in_comment;
			}
		}
		if (in_comment) {
			continue;
		}
		if (t->kind == TOKEN_IDENTIFIER) {
			if (!pattern || !strcmp(t->name.buf, pattern)) {
				found = true;
				printf("%s:%d\t%s\n", filename, t->line, t->name.buf);
			}
		}
	}
	return found;
}

static void
process_one_file(const char *filename)
{
	struct token *tokens;
	char *buffer = read_file(filename);
	if (!buffer) {
		exit(EXIT_FAILURE);
	}
	current_line = 1;
	tokens = lex(buffer);
	free(buffer);

	if (!argv_tokens) {
		/* Dump all idents */
		grep(tokens, filename, NULL);
	} else {
		for (char **p = argv_tokens; *p; p++) {
			found_token |= grep(tokens, filename, *p);
		}
	}
}

int
main(int argc, char **argv)
{
	if (argc < 2) {
		usage();
	}
	for (int i = 1; i < argc; ++i) {
		char *arg = argv[i];

		if (!strncmp(arg, "--tokens=", 9)) {
			argv_tokens = split(arg + 9, ',');
		}
		if (!strcmp(arg, "-")) {
			char *line = NULL;
			size_t len = 0;
			while ((getline(&line, &len, stdin) != -1)) {
				char *p = strrchr(line, '\n');
				if (p) {
					*p = '\0';
				}
				process_one_file(line);
			}
			free(line);
			break;
		}
		if (arg[0] != '-') {
			process_one_file(arg);
			break;
		}
	}

	if (argv_tokens) {
		for (char **p = argv_tokens; *p; p++) {
			free(*p);
		}
		free(argv_tokens);
	}

	/* return failure (1) if we have found a banned identifier */
	return found_token;
}
