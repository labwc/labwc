// SPDX-License-Identifier: GPL-2.0-only
/*
 * Convert xbm file to buffer with cairo surface
 *
 * Copyright Johan Malm 2020-2023
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <drm_fourcc.h>
#include "button/button-xbm.h"
#include "button/common.h"
#include "common/grab-file.h"
#include "common/mem.h"
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

static uint32_t color;
static char *current_buffer_position;
static struct token *tokens;
static int nr_tokens, alloc_tokens;

static void
add_token(enum token_type token_type)
{
	if (nr_tokens == alloc_tokens) {
		alloc_tokens = (alloc_tokens + 16) * 2;
		tokens = xrealloc(tokens, alloc_tokens * sizeof(struct token));
	}
	struct token *token = tokens + nr_tokens;
	memset(token, 0, sizeof(*token));
	nr_tokens++;
	token->type = token_type;
}

static void
get_identifier_token(void)
{
	struct token *token = tokens + nr_tokens - 1;
	token->name[token->pos] = current_buffer_position[0];
	token->pos++;
	if (token->pos == MAX_TOKEN_SIZE - 1) {
		return;
	}
	current_buffer_position++;
	switch (current_buffer_position[0]) {
	case '\0':
		return;
	case 'a' ... 'z':
	case 'A' ... 'Z':
	case '0' ... '9':
	case '_':
	case '#':
		get_identifier_token();
		break;
	default:
		break;
	}
}

static void
get_number_token(void)
{
	struct token *token = tokens + nr_tokens - 1;
	token->name[token->pos] = current_buffer_position[0];
	token->pos++;
	if (token->pos == MAX_TOKEN_SIZE - 1) {
		return;
	}
	current_buffer_position++;
	switch (current_buffer_position[0]) {
	case '\0':
		return;
	case '0' ... '9':
	case 'a' ... 'f':
	case 'A' ... 'F':
	case 'x':
		get_number_token();
		break;
	default:
		break;
	}
}

static void
get_special_char_token(void)
{
	struct token *token = tokens + nr_tokens - 1;
	token->name[0] = current_buffer_position[0];
	current_buffer_position++;
}

/**
 * tokenize_xbm - tokenize xbm file
 * @buffer: buffer containing xbm file
 * return token vector
 */
static struct token *
tokenize_xbm(char *buffer)
{
	tokens = NULL;
	nr_tokens = 0;
	alloc_tokens = 0;

	current_buffer_position = buffer;

	for (;;) {
		switch (current_buffer_position[0]) {
		case '\0':
			goto out;
		case 'a' ... 'z':
		case 'A' ... 'Z':
		case '_':
		case '#':
			add_token(TOKEN_IDENT);
			get_identifier_token();
			continue;
		case '0' ... '9':
			add_token(TOKEN_INT);
			get_number_token();
			struct token *token = tokens + nr_tokens - 1;
			token->value = (int)strtol(token->name, NULL, 0);
			continue;
		case '{':
			add_token(TOKEN_SPECIAL);
			get_special_char_token();
			continue;
		default:
			break;
		}
		++current_buffer_position;
	}
out:
	add_token(TOKEN_NONE); /* vector end marker */
	return tokens;
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
process_bytes(struct pixmap *pixmap, struct token *tokens)
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
parse_xbm_tokens(struct token *tokens)
{
	struct pixmap pixmap = { 0 };

	for (struct token *t = tokens; t->type; t++) {
		if (pixmap.width && pixmap.height) {
			if (t->type != TOKEN_INT) {
				continue;
			}
			process_bytes(&pixmap, t);
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
parse_xbm_builtin(const char *button, int size)
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
	t[size].type = 0;
	process_bytes(&pixmap, t);
	return pixmap;
}

void
button_xbm_from_bitmap(const char *bitmap, struct lab_data_buffer **buffer,
		float *rgba)
{
	struct pixmap pixmap = {0};
	if (*buffer) {
		wlr_buffer_drop(&(*buffer)->base);
		*buffer = NULL;
	}
	color = argb32(rgba);
	pixmap = parse_xbm_builtin(bitmap, 6);
	*buffer = buffer_create_wrap(pixmap.data, pixmap.width, pixmap.height,
		pixmap.width * 4, /* free_on_destroy */ true);
}

void
button_xbm_load(const char *button_name, struct lab_data_buffer **buffer,
		float *rgba)
{
	struct pixmap pixmap = {0};
	if (*buffer) {
		wlr_buffer_drop(&(*buffer)->base);
		*buffer = NULL;
	}
	color = argb32(rgba);

	/* Read file into memory as it's easier to tokenize that way */
	char filename[4096] = { 0 };
	button_filename(button_name, filename, sizeof(filename));
	char *token_buffer = grab_file(filename);
	if (token_buffer) {
		struct token *tokens = tokenize_xbm(token_buffer);
		free(token_buffer);
		pixmap = parse_xbm_tokens(tokens);
		if (tokens) {
			free(tokens);
		}
	}
	if (!pixmap.data) {
		return;
	}

	/* Create buffer with free_on_destroy being true */
	if (pixmap.data) {
		*buffer = buffer_create_wrap(pixmap.data, pixmap.width,
			pixmap.height, pixmap.width * 4, true);
	}
}
