#include <stddef.h>
#include <stdlib.h>

#ifndef MY_ALLOC
#define MY_ALLOC

struct alloc_t {
  void *closure;
  void *(*alloc)(int size, void *closure);
  void *(*deleter)(void *addr, void *closure);
};

void *pkt_default_alloca(const int size, void *closure) { return malloc(size); }

void pkt_default_deleter(void *p, void *closure) { free(p); }

struct alloc_t default_alloc = {.closure = NULL,
                                .alloc = pkt_default_alloca,
                                .deleter = pkt_default_deleter};

#endif
