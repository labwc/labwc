CFLAGS += -g -Wall -O0 -std=c11
LDFLAGS += -fsanitize=address

PROGS = find-idents

all: $(PROGS)

find-idents: find-idents.o
	$(CC) -o $@ $^

clean :
	$(RM) $(PROGS) *.o
