CFLAGS  = -g -O0 -Wall -Wextra -std=c11 -pedantic `xml2-config --cflags`
CFLAGS += -Wno-unused-parameter
CFLAGS += -I../../include
ASAN_FLAGS = -O0 -fsanitize=address -fno-common -fno-omit-frame-pointer -rdynamic
CFLAGS    += $(ASAN_FLAGS)
LDFLAGS   += $(ASAN_FLAGS) -fuse-ld=gold
LDFLAGS = `xml2-config --libs`

PROGS = rcxml-print-nodenames

all: $(PROGS)

rcxml-print-nodenames: rcxml-print-nodenames.c
	$(CC) $(CFLAGS) -o $@ $^ ../../src/config/rcxml.c ../../src/common/buf.c $(LDFLAGS)

clean:
	rm -f $(PROGS)
