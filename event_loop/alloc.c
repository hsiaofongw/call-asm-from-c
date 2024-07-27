#include "alloc.h"

static void *pkt_default_alloca(const int size, void *closure) {
  return malloc(size);
}

static void pkt_default_deleter(void *p, void *closure) { free(p); }

static struct alloc_t default_alloc = {.closure = NULL,
                                       .alloc = pkt_default_alloca,
                                       .deleter = pkt_default_deleter};

// align x to multiples of 2^m
int align_to(int x, int m) {
  int mask = (((int)1) << m);
  int rem = x & (mask - 1);
  int result = x - rem;
  return rem == 0 ? x - rem : x - rem + mask;
}

int align_default(int x) { return align_to(x, DEFAULT_ALIGN_M); }

int align_page(int x) { return align_to(x, PAGE_SIZE_M); }

struct alloc_t *get_default_allocator() { return &default_alloc; }