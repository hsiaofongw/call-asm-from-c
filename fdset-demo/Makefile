CC=clang-18
MUSL_PREFIX=$(HOME)/.local/musl-1.2.5
CFLAGS=-O3 -I$(MUSL_PREFIX)/include -std=c17

all: fdset_demo socket_mux

fdset_demo: fdset_demo.c
	$(CC) -o $@ $(CFLAGS) $^

socket_mux: socket_mux.o llist.o conn_manage.o
	$(CC) -flto -o $@ -L$(MUSL_PREFIX)/lib --static $^

socket_mux.o: socket_mux.c
	$(CC) -o $@ $(CFLAGS) -c $^

llist.o: llist.c
	$(CC) -o $@ $(CFLAGS) -c $^

conn_manage.o: conn_manage.c
	$(CC) -o $@ $(CFLAGS) -c $^

clean:
	rm -f fdset_demo
	rm -f socket_mux
	rm -f socket_mux.o
	rm -f llist.o
	rm -r conn_manage.o

build: fdset_demo
