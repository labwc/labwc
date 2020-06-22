#ifndef XBM_H
#define XBM_H

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
	size_t pos;
	enum token_type type;
};

/**
 * tokenize - tokenize xbm file
 * @buffer: buffer containing xbm file
 * returns vector of tokens
 */
struct token *tokenize(char *buffer);

#endif /* XBM_H */
