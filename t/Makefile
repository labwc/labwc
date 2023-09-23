CFLAGS += -g -Wall -O0 -I../include
LDFLAGS += -fsanitize=address

PROGS = t1000-buf.t

all: $(PROGS)

t1000-buf.t: t1000-buf.c tap.c ../src/common/buf.c ../src/common/mem.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) -f $(PROGS)
