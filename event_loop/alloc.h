#include <stddef.h>
#include <stdlib.h>

#ifndef MY_ALLOC
#define MY_ALLOC

#define DEFAULT_ALIGN_M 4
#define PAGE_SIZE_M 12
#define PAGE_SIZE (((int)0x1) << PAGE_SIZE_M)

struct alloc_t {
  void *closure;
  void *(*alloc)(int size, void *closure);
  void (*deleter)(void *addr, void *closure);
};

void *pkt_default_alloca(const int size, void *closure);

void pkt_default_deleter(void *p, void *closure);

struct alloc_t default_alloc;

// 把 x 向上对齐到 2 的 m 次方。
int align_to(int x, int m);

int align_default(int x);

int align_page(int x);

#endif
