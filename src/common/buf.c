// SPDX-License-Identifier: GPL-2.0-only
#include "common/buf.h"
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wlr/util/log.h>
#include "common/macros.h"
#include "common/mem.h"
#include "common/string-helpers.h"

void
buf_expand_tilde(struct buf *s)
{
	struct buf tmp = BUF_INIT;
	for (int i = 0 ; i < s->len ; i++) {
		if (s->data[i] == '~') {
			buf_add(&tmp, getenv("HOME"));
		} else {
			buf_add_char(&tmp, s->data[i]);
		}
	}
	buf_move(s, &tmp);
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
	struct buf tmp = BUF_INIT;
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
				buf_add(&tmp, p);
			}
		} else {
			buf_add_char(&tmp, s->data[i]);
		}
	}
	buf_reset(&environment_variable);
	buf_move(s, &tmp);
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
buf_add_hex_color(struct buf *s, float color[4])
{
	/*
	 * In theme.c parse_hexstr() colors are pre-multiplied (by alpha) as
	 * expected by wlr_scene(). We therefore need to reverse that here.
	 *
	 * For details, see https://github.com/labwc/labwc/pull/1685
	 */
	float alpha = color[3];

	/* Avoid division by zero */
	if (alpha == 0.0f) {
		buf_add(s, "#00000000");
		return;
	}

	buf_add_fmt(s, "#%02x%02x%02x%02x",
		(int)(color[0] / alpha * 255),
		(int)(color[1] / alpha * 255),
		(int)(color[2] / alpha * 255),
		(int)(alpha * 255));
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

struct buf
buf_from_file(const char *filename)
{
	struct buf buf = BUF_INIT;
	FILE *stream = fopen(filename, "r");
	if (!stream) {
		return buf;
	}

	if (fseek(stream, 0, SEEK_END) == -1) {
		wlr_log_errno(WLR_ERROR, "fseek(%s)", filename);
		fclose(stream);
		return buf;
	}
	long size = ftell(stream);
	if (size == -1) {
		wlr_log_errno(WLR_ERROR, "ftell(%s)", filename);
		fclose(stream);
		return buf;
	}
	rewind(stream);

	buf_expand(&buf, size + 1);
	if (fread(buf.data, 1, size, stream) == (size_t)size) {
		buf.len = size;
		buf.data[size] = '\0';
	} else {
		wlr_log_errno(WLR_ERROR, "fread(%s)", filename);
		buf_reset(&buf);
	}
	fclose(stream);
	return buf;
}
