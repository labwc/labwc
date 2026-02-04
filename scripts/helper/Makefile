CFLAGS += -g -Wall -O0 -std=c11
LDFLAGS += -fsanitize=address

PROGS = find-idents format-xml

all: $(PROGS)

find-idents: find-idents.o
	$(CC) -o $@ $^

format-xml: CFLAGS += `pkg-config --cflags libxml-2.0`
format-xml: CFLAGS += `pkg-config --libs libxml-2.0`
format-xml: format-xml.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean :
	$(RM) $(PROGS) *.o
