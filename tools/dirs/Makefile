CFLAGS  += -g -Wall -I../../include
CFLAGS  += `pkg-config --cflags glib-2.0`
LDFLAGS += `pkg-config --libs glib-2.0`

all:
	$(CC) $(CFLAGS) -o dir-list dir-list.c ../../src/common/dir.c ../../src/common/log.c $(LDFLAGS)

clean:
	rm -f *.o dir-list
