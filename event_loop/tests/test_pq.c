#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../priority_queue.h"

int less_than_or_equal(void *a, void *b, void *closure) {
  int *x = a;
  int *y = b;
  if (*x <= *y) {
    return 1;
  }
  return 0;
}

void print_buf(void **buf, int offset, int size) {
  fprintf(stderr, "Dump buf:\n");
  for (int i = 0; i < size; ++i) {
    fprintf(stderr, "0x%016lx\n", (unsigned long)buf[offset + i]);
  }
}

int main() {
  pq *p = pq_create(10, less_than_or_equal, NULL);
  if (!p) {
    fprintf(stderr, "Failed to allocate pq.\n");
    exit(1);
  }

  void **pq_buf = pq_get_buffer(p);

  int test_data[] = {3, 1, 5, 7, 4, 2};
  const int num_elems = sizeof(test_data) / sizeof(int);
  // for (int i = 0; i < num_elems; ++i) {
  //   fprintf(stderr, "Addr of %d: 0x%016lx\n", test_data[i],
  //           (unsigned long)&test_data[i]);
  // }

  for (int i = 0; i < num_elems; ++i) {
    if (!pq_is_full(p)) {
      pq_insert(p, &test_data[i]);
      fprintf(stderr, "Inserted %d\n", test_data[i]);
      // print_buf(pq_buf, 1, i + 1);
    }
  }

  while (!pq_is_empty(p)) {
    int *elem = pq_shift(p);
    if (elem) {
      fprintf(stderr, "Extracted %d\n", *elem);
    }
  }

  fprintf(stderr, "Round 2:\n");

  int i = 0;
  fprintf(stderr, "Insert %d\n", test_data[i]);
  pq_insert(p, &test_data[i++]);
  fprintf(stderr, "Insert %d\n", test_data[i]);
  pq_insert(p, &test_data[i++]);
  fprintf(stderr, "Insert %d\n", test_data[i]);
  pq_insert(p, &test_data[i++]);

  int *x;
  x = pq_shift(p);
  fprintf(stderr, "Extracted %d\n", *x);

  x = pq_shift(p);
  fprintf(stderr, "Extracted %d\n", *x);

  fprintf(stderr, "Insert %d\n", test_data[i]);
  pq_insert(p, &test_data[i++]);
  fprintf(stderr, "Insert %d\n", test_data[i]);
  pq_insert(p, &test_data[i++]);
  fprintf(stderr, "Insert %d\n", test_data[i]);
  pq_insert(p, &test_data[i++]);

  x = pq_shift(p);
  fprintf(stderr, "Extracted %d\n", *x);

  x = pq_shift(p);
  fprintf(stderr, "Extracted %d\n", *x);
  x = pq_shift(p);
  fprintf(stderr, "Extracted %d\n", *x);

  x = pq_shift(p);
  fprintf(stderr, "Extracted %d\n", *x);

  fprintf(stderr, "Insert %d\n", test_data[0]);
  pq_insert(p, &test_data[0]);

  x = pq_shift(p);
  fprintf(stderr, "Extracted %d\n", *x);

  pq_free(&p);
  if (p) {
    fprintf(stderr, "Failed to free pq.\n");
    exit(1);
  }
  return 0;
}
