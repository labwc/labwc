CFLAGS += -g -Wall -O0 -std=c11
CFLAGS += `pkg-config cairo pango pangocairo --cflags`
CFLAGS += -I../../include
LIBS   += `pkg-config cairo pango pangocairo --libs`
LDFLAGS += $(LIBS)

PROGS = font-height

all: $(PROGS)

font-height: font-height.c ../../src/common/font.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(PROGS) *.o
