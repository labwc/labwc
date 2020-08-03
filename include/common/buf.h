/*
 * Very simple C string buffer implementation
 *
 * Copyright Johan Malm 2020
 */

#ifndef __LABWC_BUF_H
#define __LABWC_BUF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct buf {
	char *buf;
	int alloc;
	int len;
};

/*
 * buf_init - allocate NULL-terminated C string buffer
 * @s - buffer
 * Note: use free(s->buf) to free it.
 */
void buf_init(struct buf *s);

/*
 * buf_add - add data to C string buffer
 * @s - buffer
 * @data - data to be added
 */
void buf_add(struct buf *s, const char *data);

#endif /* __LABWC_BUF_H */
