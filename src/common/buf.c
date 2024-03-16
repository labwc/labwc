// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include "common/buf.h"
#include "common/mem.h"

void
buf_expand_tilde(struct buf *s)
{
	struct buf new;
	buf_init(&new);
	for (int i = 0 ; i < s->len ; i++) {
		if (s->buf[i] == '~') {
			buf_add(&new, getenv("HOME"));
		} else {
			buf_add_char(&new, s->buf[i]);
		}
	}
	free(s->buf);
	s->buf = new.buf;
	s->len = new.len;
	s->alloc = new.alloc;
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
	struct buf new;
	struct buf environment_variable;
	buf_init(&new);
	buf_init(&environment_variable);

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
	free(environment_variable.buf);
	free(s->buf);
	s->buf = new.buf;
	s->len = new.len;
	s->alloc = new.alloc;
}

void
buf_init(struct buf *s)
{
	/* we can't assert(!s->buf) here because struct may be uninitialized */

	s->alloc = 256;
	s->buf = xmalloc(s->alloc);
	s->buf[0] = '\0';
	s->len = 0;
}

void
buf_add(struct buf *s, const char *data)
{
	assert(s->buf);

	if (!data || data[0] == '\0') {
		return;
	}
	int len = strlen(data);
	if (s->alloc <= s->len + len + 1) {
		s->alloc = s->alloc + len;
		s->buf = xrealloc(s->buf, s->alloc);
	}
	memcpy(s->buf + s->len, data, len);
	s->len += len;
	s->buf[s->len] = 0;
}

void
buf_add_char(struct buf *s, char ch)
{
	assert(s->buf);

	if (s->alloc <= s->len + 1) {
		s->alloc = s->alloc * 3 / 2 + 16;
		s->buf = xrealloc(s->buf, s->alloc);
	}
	s->buf[s->len++] = ch;
	s->buf[s->len] = '\0';
}

void
buf_clear(struct buf *s)
{
	assert(s->buf);

	s->len = 0;
	s->buf[0] = '\0';
}

void
buf_reset(struct buf *s)
{
	zfree(s->buf);
	buf_init(s);
}
