#include <stdlib.h>

#ifndef MY_LLIST
#define MY_LLIST

struct llist_t;
struct llist_t {
  void *payload;
  struct llist_t *next;
};

// 新建一个空的列表
struct llist_t *list_create();

// 新建一个空的列表元素
struct llist_t *list_elem_create();

// 释放一个列表元素
void list_elem_free(struct llist_t *elem,
                    void (*before_elem_delete)(void *payload, void *closure),
                    void *closure);

// 释放一整个列表，在每个元素被释放前，你有足够的时间在 before_elem_free
// 中实现对 payload 的释放。
void list_free(struct llist_t *head,
               void (*before_elem_free)(void *payload, void *closure),
               void *closure);

// 遍历列表
void list_traverse(struct llist_t *head, void *closure,
                   int (*cb)(struct llist_t *curr_elem, int idx,
                             void *closure));

// 返回列表当前长度
size_t list_get_size(struct llist_t *head);

// 插入一个列表元素到首端
struct llist_t *list_insert_elem(struct llist_t *l, struct llist_t *elem);

// 提供一个 payload，并自动在列表首端插入一个包含了这个 payload 的列表元素。
struct llist_t *list_insert_payload(struct llist_t *l, void *payload);

void list_elem_find_and_remove(
    struct llist_t **root, void *predicate_closure,
    int (*predicate)(void *payload, int idx, void *predicate_closure),
    void *before_elem_delete_closure,
    void (*before_elem_free)(void *payload, void *before_elem_delete_closure));

#endif