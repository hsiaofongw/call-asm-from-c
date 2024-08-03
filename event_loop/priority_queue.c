#include "priority_queue.h"

#include <stdlib.h>
#include <string.h>

struct pq_impl {
  int capacity;
  int max_idx;
  void **elems;
  int (*cmp)(void *a, void *b, void *closure);
  void *cmp_closure;
};

pq *pq_create(int m, int (*cmp)(void *a, void *b, void *closure),
              void *cmp_closure) {
  pq *p = malloc(sizeof(pq));
  if (!p) {
    return NULL;
  }

  int cap = 1 << m;
  int bufsize = cap + 1;
  p->elems = malloc(bufsize * sizeof(void *));
  if (!p->elems) {
    free(p);
    return NULL;
  }

  p->capacity = cap;
  p->max_idx = 0;
  p->cmp = cmp;
  p->cmp_closure = cmp_closure;
  return p;
}

void pq_free(pq **p_ptr) {
  if (!p_ptr) {
    return;
  }

  pq *p = *p_ptr;
  if (!p) {
    return;
  }

  if (p->elems) {
    free(p->elems);
    p->elems = NULL;
  }
  free(p);
  *p_ptr = NULL;
}

int pq_get_capacity(pq *p) { return p->capacity; }

int pq_is_full(pq *p) { return p->max_idx == p->capacity; }

int pq_is_empty(pq *p) { return p->max_idx == 0; }

int pq_upscale(pq *p, int m) {
  int new_cap = 1 << m;
  if (new_cap <= p->capacity) {
    return 0;
  }

  int new_bufsize = new_cap + 1;
  void **new_buf = malloc(new_bufsize * sizeof(void *));
  if (!new_buf) {
    return -1;
  }

  memcpy(new_buf, p->elems, (p->capacity + 1) * sizeof(void *));
  p->capacity = new_cap;
  free(p->elems);
  p->elems = new_buf;
  return 0;
}

void exch_elem(void **ary, int i, int j) {
  void *tmp = ary[i];
  ary[i] = ary[j];
  ary[j] = tmp;
}

void heap_float(pq *p) {
  int curr = p->max_idx;
  while (curr > 1) {
    int parent = curr / 2;
    if (p->cmp(p->elems[parent], p->elems[curr], p->cmp_closure)) {
      break;
    }
    exch_elem(p->elems, curr, parent);
    curr = parent;
  }
}

int compare_and_exchange(pq *p, int *curr, int child) {
  if (child <= p->max_idx) {
    if (!p->cmp(p->elems[*curr], p->elems[child], p->cmp_closure)) {
      exch_elem(p->elems, *curr, child);
      *curr = child;
      return 1;
    }
  }
  return 0;
}

void heap_sink(pq *p) {
  int curr = 1;
  while (2 * curr <= p->max_idx) {
    int child_idx = curr * 2;
    if (child_idx < p->max_idx &&
        !p->cmp(p->elems[child_idx], p->elems[child_idx + 1], p->cmp_closure)) {
      ++child_idx;
    }
    if (p->cmp(p->elems[curr], p->elems[child_idx], p->cmp_closure)) {
      break;
    }
    exch_elem(p->elems, curr, child_idx);
    curr = child_idx;
  }
}

void pq_insert(pq *p, void *elem) {
  ++(p->max_idx);
  p->elems[p->max_idx] = elem;
  heap_float(p);
}

void *pq_shift(pq *p) {
  exch_elem(p->elems, 1, p->max_idx);
  void *result = p->elems[p->max_idx];
  --(p->max_idx);
  heap_sink(p);
  return result;
}

void **pq_get_buffer(pq *p) { return p->elems; }