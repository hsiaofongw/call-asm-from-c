CC=clang-18
MUSL_PREFIX=$(HOME)/.local/musl-1.2.5
CFLAGS=-O3 -I$(MUSL_PREFIX)/include -std=c17

all: fdset_demo socket_mux io_echo

alloc.o: alloc.c
	clang-18 -O3 -flto -c -o $@ $^

err.o: err.c
	clang-18 -O3 -flto -c -o $@ $^

blob.o: blob.c
	clang-18 -O3 -flto -c -o $@ $^

chat_room: chat_room.c llist.c ringbuf.c util.c
	clang-18 -O3 -flto -o $@ $^ $(shell pkg-config --cflags --libs libevent)

chat_room_dbg: chat_room.c llist.c ringbuf.c util.c
	clang-18 -O0 -g3 -o $@ $^ $(shell pkg-config --cflags --libs libevent)

io_echo: io_echo.c util.c
	$(CC) -o $@ -O3 -flto $^ $(shell pkg-config --cflags --libs libevent)

fdset_demo: fdset_demo.c
	$(CC) -flto -o $@ $(CFLAGS) -L$(MUSL_PREFIX)/lib --static $^

socket_mux: socket_mux.o llist.o conn_manage.o util.o
	$(CC) -flto -o $@ -L$(MUSL_PREFIX)/lib --static $^

socket_mux.o: socket_mux.c
	$(CC) -o $@ $(CFLAGS) -c $^

util.o: util.c
	$(CC) -o $@ -O3 -std=c17 -c $^

llist.o: llist.c
	$(CC) -o $@ $(CFLAGS) -c $^

conn_manage.o: conn_manage.c
	$(CC) -o $@ $(CFLAGS) -c $^

clean:
	rm -f fdset_demo
	rm -f socket_mux
	rm -f socket_mux.o
	rm -f llist.o
	rm -f conn_manage.o
	rm -f io_echo
	rm -f io_echo.o
	rm -f util.o
	rm -f chat_room
	rm -f chat_room_dbg
	rm -f alloc.o

build: fdset_demo
