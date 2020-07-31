CFLAGS  = -g -O0 -Wall -Wextra -std=c11 -pedantic `xml2-config --cflags`
CFLAGS += -Wno-unused-parameter
CFLAGS += -I../../include
CFLAGS += -DWLR_USE_UNSTABLE
ASAN_FLAGS = -O0 -fsanitize=address -fno-common -fno-omit-frame-pointer -rdynamic
CFLAGS    += $(ASAN_FLAGS)
LDFLAGS   += $(ASAN_FLAGS) -fuse-ld=gold
LDFLAGS   += `xml2-config --libs`
LDFLAGS   += `pkg-config --cflags --libs glib-2.0 wayland-server xkbcommon`

PROGS = rcxml-print-nodenames
SRC = \
	rcxml-print-nodenames.c \
	../../src/config/rcxml.c \
	../../src/config/config-dir.c \
	../../src/common/buf.c \
	../../src/config/keybind.c


all: $(PROGS)

rcxml-print-nodenames: $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(PROGS)
