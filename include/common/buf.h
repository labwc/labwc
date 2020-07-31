/*
 * Very simple C buffer implementation
 *
 * Copyright Johan Malm 2020
 */

#ifndef BUF_H
#define BUF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct buf {
	char *buf;
	int alloc;
	int len;
};

void buf_init(struct buf *s);
void buf_add(struct buf *s, const char *data);

#endif /* BUF_H */
