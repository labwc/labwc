// SPDX-License-Identifier: GPL-2.0-only
/*
 * Convert xbm file to buffer with cairo surface
 *
 * Copyright Johan Malm 2020-2023
 */

#define _POSIX_C_SOURCE 200809L
#include "img/img-xbm.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common/buf.h"
#include "common/mem.h"
#include "common/string-helpers.h"
#include "buffer.h"

enum token_type {
	TOKEN_NONE = 0,
	TOKEN_IDENT,
	TOKEN_INT,
	TOKEN_SPECIAL,
	TOKEN_OTHER,
};

#define MAX_TOKEN_SIZE (256)
struct token {
	char name[MAX_TOKEN_SIZE];
	int value;
	size_t pos;
	enum token_type type;
};

struct pixmap {
	uint32_t *data;
	int width;
	int height;
};

struct tokenizer_context {
	char *buffer_position;
	struct token *tokens;
	int nr_tokens, alloc_tokens;
};

static void
add_token(struct tokenizer_context *ctx, enum token_type token_type)
{
	if (ctx->nr_tokens == ctx->alloc_tokens) {
		ctx->alloc_tokens = (ctx->alloc_tokens + 16) * 2;
		ctx->tokens = xrealloc(ctx->tokens,
			ctx->alloc_tokens * sizeof(struct token));
	}
	struct token *token = ctx->tokens + ctx->nr_tokens;
	memset(token, 0, sizeof(*token));
	ctx->nr_tokens++;
	token->type = token_type;
}

static void
get_identifier_token(struct tokenizer_context *ctx)
{
	struct token *token = ctx->tokens + ctx->nr_tokens - 1;
	token->name[token->pos] = ctx->buffer_position[0];
	token->pos++;
	if (token->pos == MAX_TOKEN_SIZE - 1) {
		return;
	}
	ctx->buffer_position++;
	switch (ctx->buffer_position[0]) {
	case '\0':
		return;
	case 'a' ... 'z':
	case 'A' ... 'Z':
	case '0' ... '9':
	case '_':
	case '#':
		get_identifier_token(ctx);
		break;
	default:
		break;
	}
}

static void
get_number_token(struct tokenizer_context *ctx)
{
	struct token *token = ctx->tokens + ctx->nr_tokens - 1;
	token->name[token->pos] = ctx->buffer_position[0];
	token->pos++;
	if (token->pos == MAX_TOKEN_SIZE - 1) {
		return;
	}
	ctx->buffer_position++;
	switch (ctx->buffer_position[0]) {
	case '\0':
		return;
	case '0' ... '9':
	case 'a' ... 'f':
	case 'A' ... 'F':
	case 'x':
		get_number_token(ctx);
		break;
	default:
		break;
	}
}

static void
get_special_char_token(struct tokenizer_context *ctx)
{
	struct token *token = ctx->tokens + ctx->nr_tokens - 1;
	token->name[0] = ctx->buffer_position[0];
	ctx->buffer_position++;
}

/**
 * tokenize_xbm - tokenize xbm file
 * @buffer: buffer containing xbm file
 * return token vector
 */
static struct token *
tokenize_xbm(char *buffer)
{
	struct tokenizer_context ctx = {0};
	ctx.buffer_position = buffer;

	for (;;) {
		switch (ctx.buffer_position[0]) {
		case '\0':
			goto out;
		case 'a' ... 'z':
		case 'A' ... 'Z':
		case '_':
		case '#':
			add_token(&ctx, TOKEN_IDENT);
			get_identifier_token(&ctx);
			continue;
		case '0' ... '9': {
			add_token(&ctx, TOKEN_INT);
			get_number_token(&ctx);
			struct token *token = ctx.tokens + ctx.nr_tokens - 1;
			token->value = (int)strtol(token->name, NULL, 0);
			continue;
		}
		case '{':
			add_token(&ctx, TOKEN_SPECIAL);
			get_special_char_token(&ctx);
			continue;
		default:
			break;
		}
		++ctx.buffer_position;
	}
out:
	add_token(&ctx, TOKEN_NONE); /* vector end marker */
	return ctx.tokens;
}

static uint32_t
argb32(float *rgba)
{
	uint32_t r[4] = { 0 };
	for (int i = 0; i < 4; i++) {
		r[i] = rgba[i] * 255;
	}
	return ((r[3] & 0xff) << 24) | ((r[0] & 0xff) << 16) |
		((r[1] & 0xff) << 8) | (r[2] & 0xff);
}

static void
process_bytes(struct pixmap *pixmap, struct token *tokens, uint32_t color)
{
	pixmap->data = znew_n(uint32_t, pixmap->width * pixmap->height);
	struct token *t = tokens;
	for (int row = 0; row < pixmap->height; row++) {
		int byte = 1;
		for (int col = 0; col < pixmap->width; col++) {
			if (col == byte * 8) {
				++byte;
				++t;
			}
			if (!t->type) {
				return;
			}
			if (t->type != TOKEN_INT) {
				return;
			}
			int bit = 1 << (col % 8);
			if (t->value & bit) {
				pixmap->data[row * pixmap->width + col] = color;
			}
		}
		++t;
	}
}

/**
 * parse_xbm_tokens - parse xbm tokens and create pixmap
 * @tokens: token vector
 */
static struct pixmap
parse_xbm_tokens(struct token *tokens, uint32_t color)
{
	struct pixmap pixmap = { 0 };

	for (struct token *t = tokens; t->type; t++) {
		if (pixmap.width && pixmap.height) {
			if (t->type != TOKEN_INT) {
				continue;
			}
			process_bytes(&pixmap, t, color);
			goto out;
		}
		if (strstr(t->name, "width")) {
			pixmap.width = atoi((++t)->name);
		} else if (strstr(t->name, "height")) {
			pixmap.height = atoi((++t)->name);
		}
	}
out:
	return pixmap;
}

/*
 * Openbox built-in icons are not bigger than 8x8, so have only written this
 * function to cope wit that max size
 */
#define LABWC_BUILTIN_ICON_MAX_SIZE (8)

/**
 * parse_xbm_builtin - parse builtin xbm button and create pixmap
 * @button: button byte array (xbm format)
 */
static struct pixmap
parse_xbm_builtin(const char *button, int size, uint32_t color)
{
	struct pixmap pixmap = { 0 };

	assert(size <= LABWC_BUILTIN_ICON_MAX_SIZE);
	pixmap.width = size;
	pixmap.height = size;

	struct token t[LABWC_BUILTIN_ICON_MAX_SIZE + 1];
	for (int i = 0; i < size; i++) {
		t[i].value = button[i];
		t[i].type = TOKEN_INT;
	}
	t[size].type = TOKEN_NONE;
	process_bytes(&pixmap, t, color);
	return pixmap;
}

struct lab_data_buffer *
img_xbm_load_from_bitmap(const char *bitmap, float *rgba)
{
	uint32_t color = argb32(rgba);
	struct pixmap pixmap = parse_xbm_builtin(bitmap, 6, color);

	return buffer_create_from_data(pixmap.data, pixmap.width, pixmap.height,
		pixmap.width * 4);
}

struct lab_data_buffer *
img_xbm_load(const char *filename, float *rgba)
{
	struct pixmap pixmap = {0};
	if (string_null_or_empty(filename)) {
		return NULL;
	}
	uint32_t color = argb32(rgba);

	/* Read file into memory as it's easier to tokenize that way */
	struct buf token_buf = buf_from_file(filename);
	if (token_buf.len) {
		struct token *tokens = tokenize_xbm(token_buf.data);
		pixmap = parse_xbm_tokens(tokens, color);
		if (tokens) {
			free(tokens);
		}
	}
	buf_reset(&token_buf);
	if (!pixmap.data) {
		return NULL;
	}

	return buffer_create_from_data(pixmap.data, pixmap.width,
		pixmap.height, pixmap.width * 4);
}
