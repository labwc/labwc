// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "common/buf.h"
#include "common/macros.h"
#include "common/mem.h"
#include "common/string-helpers.h"

void
buf_expand_tilde(struct buf *s)
{
	struct buf new = BUF_INIT;
	for (int i = 0 ; i < s->len ; i++) {
		if (s->data[i] == '~') {
			buf_add(&new, getenv("HOME"));
		} else {
			buf_add_char(&new, s->data[i]);
		}
	}
	buf_move(s, &new);
}

static void
strip_curly_braces(char *s)
{
	size_t len = strlen(s);
	if (s[0] != '{' || s[len - 1] != '}') {
		return;
	}
	len -= 2;
	memmove(s, s + 1, len);
	s[len] = 0;
}

static bool
isvalid(char p)
{
	return isalnum(p) || p == '_' || p == '{' || p == '}';
}

void
buf_expand_shell_variables(struct buf *s)
{
	struct buf new = BUF_INIT;
	struct buf environment_variable = BUF_INIT;

	for (int i = 0 ; i < s->len ; i++) {
		if (s->data[i] == '$' && isvalid(s->data[i+1])) {
			/* expand environment variable */
			buf_clear(&environment_variable);
			buf_add(&environment_variable, s->data + i + 1);
			char *p = environment_variable.data;
			while (isvalid(*p)) {
				++p;
			}
			*p = '\0';
			i += strlen(environment_variable.data);
			strip_curly_braces(environment_variable.data);
			p = getenv(environment_variable.data);
			if (p) {
				buf_add(&new, p);
			}
		} else {
			buf_add_char(&new, s->data[i]);
		}
	}
	buf_reset(&environment_variable);
	buf_move(s, &new);
}

static void
buf_expand(struct buf *s, int new_alloc)
{
	/*
	 * "s->alloc &&" ensures that s->data is always allocated after
	 * returning (even if new_alloc == 0). The extra check is not
	 * really necessary but makes it easier for analyzers to see
	 * that we never overwrite a string literal.
	 */
	if (s->alloc && new_alloc <= s->alloc) {
		return;
	}
	new_alloc = MAX(new_alloc, 256);
	new_alloc = MAX(new_alloc, s->alloc * 3 / 2);
	if (s->alloc) {
		assert(s->data);
		s->data = xrealloc(s->data, new_alloc);
	} else {
		assert(!s->len);
		s->data = xmalloc(new_alloc);
		s->data[0] = '\0';
	}
	s->alloc = new_alloc;
}

void
buf_add_fmt(struct buf *s, const char *fmt, ...)
{
	if (string_null_or_empty(fmt)) {
		return;
	}
	va_list ap;

	va_start(ap, fmt);
	int n = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	if (n < 0) {
		return;
	}

	size_t size = (size_t)n + 1;
	buf_expand(s, s->len + size);

	va_start(ap, fmt);
	n = vsnprintf(s->data + s->len, size, fmt, ap);
	va_end(ap);

	if (n < 0) {
		return;
	}

	s->len += n;
	s->data[s->len] = 0;
}

void
buf_add(struct buf *s, const char *data)
{
	if (string_null_or_empty(data)) {
		return;
	}
	int len = strlen(data);
	buf_expand(s, s->len + len + 1);
	memcpy(s->data + s->len, data, len);
	s->len += len;
	s->data[s->len] = 0;
}

void
buf_add_char(struct buf *s, char ch)
{
	buf_expand(s, s->len + 2);
	s->data[s->len++] = ch;
	s->data[s->len] = '\0';
}

void
buf_clear(struct buf *s)
{
	if (s->alloc) {
		assert(s->data);
		s->len = 0;
		s->data[0] = '\0';
	} else {
		*s = BUF_INIT;
	}
}

void
buf_reset(struct buf *s)
{
	if (s->alloc) {
		free(s->data);
	}
	*s = BUF_INIT;
}

void
buf_move(struct buf *dst, struct buf *src)
{
	if (dst->alloc) {
		free(dst->data);
	}
	*dst = *src;
	*src = BUF_INIT;
}
