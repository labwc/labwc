CFLAGS  += -g -O3 -Wall -std=c99 -I. -DWLR_USE_UNSTABLE
CFLAGS  += `pkg-config --cflags wlroots wayland-server xkbcommon`
CFLAGS  += -Wextra -Wno-format-zero-length -Wold-style-definition -Woverflow \
           -Wpointer-arith -Wstrict-prototypes -Wvla -Wunused-result \
           -Wno-unused-parameter

LDFLAGS += `pkg-config --libs wlroots wayland-server xkbcommon`

ASAN_FLAGS = -O0 -fsanitize=address -fno-common -fno-omit-frame-pointer -rdynamic
CFLAGS    += $(ASAN_FLAGS)
LDFLAGS   += $(ASAN_FLAGS) -fuse-ld=gold

WP = `pkg-config --variable=pkgdatadir wayland-protocols`
WS = `pkg-config --variable=wayland_scanner wayland-scanner`

all: tinywl

tinywl: xdg-shell-protocol.o tinywl.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

xdg-shell-protocol.h:
	$(WS) server-header $(WP)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-protocol.c: xdg-shell-protocol.h
	$(WS) private-code $(WP)/stable/xdg-shell/xdg-shell.xml $@

clean:
	rm -f tinywl xdg-shell-protocol.* *.o
