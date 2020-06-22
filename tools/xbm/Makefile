CFLAGS      += -g -Wall -O0 -std=c11
CFLAGS      += -I../../include
LDFLAGS     += `pkg-config --cflags --libs cairo`
ASAN        += -fsanitize=address

PROGS        = xbm-tokenize
DEP_TOKENIZE = ../../src/common/buf.c ../../src/theme/xbm/tokenize.c

all: $(PROGS)

xbm-tokenize: xbm-tokenize.c $(DEP_TOKENIZE)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(ASAN)

clean :
	$(RM) $(PROGS)
