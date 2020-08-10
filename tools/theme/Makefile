CFLAGS  = -g -Wall -Wextra -pedantic -std=c11
CFLAGS += -I../../include/
CFLAGS += `pkg-config --cflags glib-2.0`
ASAN_FLAGS = -O0 -fsanitize=address -fno-common -fno-omit-frame-pointer -rdynamic
CFLAGS    += $(ASAN_FLAGS)
LDFLAGS   += $(ASAN_FLAGS) -fuse-ld=gold
LDFLAGS   += `pkg-config --cflags --libs glib-2.0 wlroots wayland-server`
LDFLAGS   += -DWLR_USE_UNSTABLE

SRCS = \
	theme-helper.c \
	../../src/theme/theme.c \
	../../src/common/dir.c

all:
	gcc $(CFLAGS) -o theme-helper $(SRCS) $(LDFLAGS)


test:
	XDG_DATA_HOME=../../data ./theme-helper labwc-default

clean:
	rm -f theme-helper *.o
