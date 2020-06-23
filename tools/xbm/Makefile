CFLAGS      += -g -Wall -O0 -std=c11
CFLAGS      += -I../../include
LDFLAGS     += `pkg-config --cflags --libs cairo`
ASAN        += -fsanitize=address

PROGS        = xbm-tokenize xbm-parse
DEP_TOKENIZE = ../../src/common/buf.c ../../src/theme/xbm/tokenize.c
DEP_PARSE    = $(DEP_TOKENIZE) ../../src/theme/xbm/parse.c

all: $(PROGS)

xbm-tokenize: xbm-tokenize.c $(DEP_TOKENIZE)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(ASAN)

xbm-parse: xbm-parse.c $(DEP_PARSE)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(ASAN)

clean :
	$(RM) $(PROGS)
