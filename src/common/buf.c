// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
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
		if (s->buf[i] == '~') {
			buf_add(&new, getenv("HOME"));
		} else {
			buf_add_char(&new, s->buf[i]);
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
		if (s->buf[i] == '$' && isvalid(s->buf[i+1])) {
			/* expand environment variable */
			buf_clear(&environment_variable);
			buf_add(&environment_variable, s->buf + i + 1);
			char *p = environment_variable.buf;
			while (isvalid(*p)) {
				++p;
			}
			*p = '\0';
			i += strlen(environment_variable.buf);
			strip_curly_braces(environment_variable.buf);
			p = getenv(environment_variable.buf);
			if (p) {
				buf_add(&new, p);
			}
		} else {
			buf_add_char(&new, s->buf[i]);
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
		assert(s->buf);
		s->buf = xrealloc(s->buf, new_alloc);
	} else {
		assert(!s->len);
		s->buf = xmalloc(new_alloc);
		s->buf[0] = '\0';
	}
	s->alloc = new_alloc;
}

void
buf_add(struct buf *s, const char *data)
{
	if (string_null_or_empty(data)) {
		return;
	}
	int len = strlen(data);
	buf_expand(s, s->len + len + 1);
	memcpy(s->buf + s->len, data, len);
	s->len += len;
	s->buf[s->len] = 0;
}

void
buf_add_char(struct buf *s, char ch)
{
	buf_expand(s, s->len + 1);
	s->buf[s->len++] = ch;
	s->buf[s->len] = '\0';
}

void
buf_clear(struct buf *s)
{
	if (s->alloc) {
		assert(s->buf);
		s->len = 0;
		s->buf[0] = '\0';
	} else {
		*s = BUF_INIT;
	}
}

void
buf_reset(struct buf *s)
{
	if (s->alloc) {
		free(s->buf);
	}
	*s = BUF_INIT;
}

void
buf_move(struct buf *dst, struct buf *src)
{
	if (dst->alloc) {
		free(dst->buf);
	}
	*dst = *src;
	*src = BUF_INIT;
}
