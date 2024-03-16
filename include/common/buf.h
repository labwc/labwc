/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Very simple C string buffer implementation
 *
 * Copyright Johan Malm 2020
 */

#ifndef LABWC_BUF_H
#define LABWC_BUF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct buf {
	char *buf;
	int alloc;
	int len;
};

/**
 * buf_expand_tilde - expand ~ in buffer
 * @s: buffer
 */
void buf_expand_tilde(struct buf *s);

/**
 * buf_expand_shell_variables - expand $foo and ${foo} in buffer
 * @s: buffer
 * Note: $$ is not handled
 */
void buf_expand_shell_variables(struct buf *s);

/**
 * buf_init - allocate NULL-terminated C string buffer
 * @s: buffer
 * Note: use buf_finish(buf) to free it
 */
void buf_init(struct buf *s);

/**
 * buf_add - add data to C string buffer
 * @s: buffer
 * @data: data to be added
 */
void buf_add(struct buf *s, const char *data);

/**
 * buf_add_char - add single char to C string buffer
 * @s: buffer
 * @data: char to be added
 */
void buf_add_char(struct buf *s, char data);

/**
 * buf_clear - clear the buffer, internal allocations are preserved
 * @s: buffer
 *
 * This is the appropriate function to call to re-use the buffer
 * in a loop or similar situations as it simply reuses the already
 * existing heap allocations.
 */
void buf_clear(struct buf *s);

/**
 * buf_reset - reset the buffer, internal allocations are free'd
 * @s: buffer
 *
 * The buffer will automatically be re-initialized so can be used again.
 *
 * This function should not be used in loops as it will free and then
 * re-allocate heap memory. However, if the buffer becomes too big
 * and the internal allocation should be free'd this is the correct
 * function to use.
 */
void buf_reset(struct buf *s);

/**
 * buf_finish - free all internal allocations
 * @s: buffer
 *
 * The state of the buffer after this call is undefined.
 * It can be re-initalized via buf_init() however.
 *
 * Use this for the usual case of a stack allocated buf instance:
 *   struct buf buf;
 *   buf_init(&buf);
 *   buf_add(&buf, "some");
 *   buf_add(&buf, "thing");
 *   do_something_with(buf.buf);
 *   buf_finish(&buf);
 */
void buf_finish(struct buf *s);

#endif /* LABWC_BUF_H */
