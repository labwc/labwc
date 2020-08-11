/*
 * Parse xbm token to create pixmap
 *
 * Copyright Johan Malm 2020
 */

#ifndef __LABWC_PARSE_H
#define __LABWC_PARSE_H

#include <stdint.h>
#include "theme/xbm/tokenize.h"

struct pixmap {
	uint32_t *data;
	int width;
	int height;
};

/**
 * parse_xbm_tokens - parse xbm tokens and create pixmap
 * @tokens: token vector
 */
struct pixmap parse_xbm_tokens(struct token *tokens);

/**
 * parse_xbm_builtin - parse builtin xbm button and create pixmap
 * @button: button byte array (xbm format)
 */
struct pixmap parse_xbm_builtin(const char *button, int size);

#endif /* __LABWC_PARSE_H */
