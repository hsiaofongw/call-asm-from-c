#include "llist.h"

#include <memory.h>
#include <stdlib.h>
#include <sys/types.h>

struct llist_t *list_create() { return NULL; }

struct llist_t *list_elem_create() { return malloc(sizeof(struct llist_t)); }

void list_elem_free(struct llist_t *elem,
                    void (*before_elem_delete)(void *payload, void *closure),
                    void *closure) {
  before_elem_delete(elem->payload, closure);
  free(elem);
}

void list_traverse(struct llist_t *head, void *closure,
                   int (*cb)(struct llist_t *curr_elem, int idx,
                             void *closure)) {
  int idx = 0;
  for (struct llist_t *curr = head; curr != NULL; curr = curr->next) {
    int keep_going = cb(curr, idx++, closure);
    if (!keep_going) {
      break;
    }
  }
}

int list_flatten_impl_elem_accessor(struct llist_t *curr, int idx,
                                    void *closoure) {
  struct llist_t **array_of_ptrs = closoure;
  array_of_ptrs[idx] = curr;
  return 1;
}

void list_flatten(struct llist_t *head, struct llist_t **dst) {
  list_traverse(head, (void *)dst, list_flatten_impl_elem_accessor);
}

void list_free(struct llist_t *head,
               void (*before_elem_free)(void *payload, void *closure),
               void *closure) {
  size_t size = list_get_size(head);
  struct llist_t **array_of_ptrs = malloc(size * sizeof(struct llist_t *));
  list_flatten(head, array_of_ptrs);
  for (int i = 0; i < size; ++i) {
    struct llist_t *elem = array_of_ptrs[i];
    list_elem_free(elem, before_elem_free, closure);
  }
  free(array_of_ptrs);
}

size_t list_get_size(struct llist_t *head) {
  ssize_t s = 0;
  for (struct llist_t *curr = head; curr != NULL; curr = curr->next) {
    ++s;
  }
  return s;
}

struct llist_t *list_insert_elem(struct llist_t *l, struct llist_t *elem) {
  elem->next = l;
  return elem;
}

struct llist_t *list_insert_payload(struct llist_t *l, void *payload) {
  struct llist_t *elem = list_elem_create();
  elem->payload = payload;
  return list_insert_elem(l, elem);
}

void list_elem_find_and_remove(
    struct llist_t **root, void *predicate_closure,
    int (*predicate)(void *payload, int idx, void *predicate_closure),
    void *before_elem_delete_closure,
    void (*before_elem_free)(void *payload, void *before_elem_delete_closure)) {
  int idx;
  struct llist_t *head = *root;
  struct llist_t *prev, *curr;
  for (idx = 0, prev = NULL, curr = head; curr != NULL;
       curr = curr->next, ++idx) {
    if (predicate(curr->payload, idx, predicate_closure)) {
      if (prev != NULL) {
        prev->next = curr->next;
      } else {
        *root = curr->next;
      }
      list_elem_free(curr, before_elem_free, before_elem_delete_closure);
      continue;
    }
    prev = curr;
  }
}