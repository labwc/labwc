#include "buf.h"

void buf_init(struct buf *s)
{
	s->alloc = 256;
	s->buf = malloc(s->alloc);
	s->buf[0] = '\0';
	s->len = 0;
}

void buf_add(struct buf *s, const char *data)
{
	if (!data || data[0] == '\0')
		return;
	int len = strlen(data);
	if (s->alloc <= s->len + len + 1) {
		s->alloc = s->alloc + len;
		s->buf = realloc(s->buf, s->alloc);
	}
	memcpy(s->buf + s->len, data, len);
	s->len += len;
	s->buf[s->len] = 0;
}
