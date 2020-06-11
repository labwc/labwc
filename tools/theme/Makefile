CFLAGS  = -g -Wall -Wextra -pedantic -std=c11
CFLAGS += -I../../include/
ASAN_FLAGS = -O0 -fsanitize=address -fno-common -fno-omit-frame-pointer -rdynamic
CFLAGS    += $(ASAN_FLAGS)
LDFLAGS   += $(ASAN_FLAGS) -fuse-ld=gold

all:
	gcc $(CFLAGS) -o theme-helper theme-helper.c ../../src/theme/theme.c $(LDFLAGS)
