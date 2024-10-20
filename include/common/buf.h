/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Very simple C string buffer implementation
 *
 * Copyright Johan Malm 2020
 */

#ifndef LABWC_BUF_H
#define LABWC_BUF_H

struct buf {
	/**
	 * Pointer to underlying string buffer. If alloc != 0, then
	 * this was allocated via malloc().
	 */
	char *data;
	/**
	 * Allocated length of buf. If zero, data was not allocated
	 * (either NULL or literal empty string).
	 */
	int alloc;
	/**
	 * Length of string contents (not including terminating NUL).
	 * Currently this must be zero if alloc is zero (i.e. non-empty
	 * literal strings are not allowed).
	 */
	int len;
};

/** Value used to initialize a struct buf to an empty string */
#define BUF_INIT ((struct buf){.data = ""})

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
 * buf_add_fmt - add format string to C string buffer
 * @s: buffer
 * @fmt: format string to be added
 */
void buf_add_fmt(struct buf *s, const char *fmt, ...);

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
 * The buffer will be set to a NUL-terminated empty string.
 *
 * This is the appropriate function to call to re-use the buffer
 * in a loop or similar situations as it reuses the existing heap
 * allocation.
 */
void buf_clear(struct buf *s);

/**
 * buf_reset - reset the buffer, internal allocations are free'd
 * @s: buffer
 *
 * The buffer will be re-initialized to BUF_INIT (empty string).
 *
 * Inside a loop, consider using buf_clear() instead, as it allows
 * reusing the existing heap allocation. buf_reset() should still be
 * called after exiting the loop.
 */
void buf_reset(struct buf *s);

/**
 * buf_move - move the contents of src to dst, freeing any previous
 * allocation of dst and resetting src to BUF_INIT.
 *
 * dst must either have been initialized with BUF_INIT
 * or zeroed out (e.g. created by znew() or on the stack
 * with something like struct buf foo = {0}).
 */
void buf_move(struct buf *dst, struct buf *src);

#endif /* LABWC_BUF_H */
