#include <stdlib.h>

#ifndef MY_LLIST
#define MY_LLIST

struct llist_impl_t;
typedef struct llist_impl_t llist_t;

// 新建一个空的列表
llist_t *list_create();

// 新建一个空的列表元素
llist_t *list_elem_create();

// 释放一个列表元素
void list_elem_free(llist_t *elem,
                    void (*before_elem_delete)(void *payload, void *closure),
                    void *closure);

// 释放一整个列表，在每个元素被释放前，你有足够的时间在 before_elem_free
// 中实现对 payload 的释放。
void list_free(llist_t *head,
               void (*before_elem_free)(void *payload, void *closure),
               void *closure);

// 遍历列表
void list_traverse(llist_t *head, void *closure,
                   int (*cb)(llist_t *curr_elem, int idx, void *closure));

// 遍历列表，但是 accessor 每次得到的是 payload 而不是 element.（payload
// 实际上是 element->payload）
void list_traverse_payload(llist_t *head, void *closure,
                           int (*cb)(void *payload, int idx, void *closure));

// 返回列表当前长度
size_t list_get_size(llist_t *head);

// 插入一个列表元素到首端
llist_t *list_insert_elem(llist_t *l, llist_t *elem);

// 提供一个 payload，并自动在列表首端插入一个包含了这个 payload 的列表元素。
llist_t *list_insert_payload(llist_t *l, void *payload);

void list_elem_find_and_remove(
    llist_t **root, void *predicate_closure,
    int (*predicate)(void *payload, int idx, void *predicate_closure),
    void *before_elem_delete_closure,
    void (*before_elem_free)(void *payload, void *before_elem_delete_closure),
    int delete_all);

#endif