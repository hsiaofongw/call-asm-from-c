#ifndef MY_PQ
#define MY_PQ

struct pq_impl;
typedef struct pq_impl pq;

// 创建一个 priority queue 对象，最大容量 2^m，分配失败时返回 NULL。
// cmp 在 a <= b 时，返回非 0 值，否则返回 0。
// closure 是显示闭包捕获对象，在 cmp 被调用时，作为参数传给 cmp。
pq *pq_create(int m, int (*cmp)(void *a, void *b, void *closure),
              void *closure);

// 释放一个 priority queue 对象
void pq_free(pq **);

// 获取 priority queue 最大容量，在需要判断是否需要扩容时会有用。
int pq_get_capacity(pq *);

// 判断 priority queue 是否已满，当 priority queue 已满时，再继续执行 insert
// 操作属于未定义行为。
int pq_is_full(pq *);

// 判断 priority queue
// 是否为空，为空指的是它从未被插入任何元素，或者插入的任何元素都已经取出。
// 当 priority queue 为空时，再继续执行 shift 操作属于未定义行为。（我们不用
// NULL 返回结果表示空）
int pq_is_empty(pq *);

// 扩展 priority queue 的最大容量至 2^m，如果 2^m
// 不大于当前的最大容量则这会是一个 no-op。出错时返回非 0 值。
int pq_upscale(pq *, int m);

// 在 priority queue 插入一个元素。
// 此函数不会夺取 elem 的所有权，调用者仍需要自己释放 elem。
void pq_insert(pq *, void *elem);

// 从 priority queue 队首取出一个元素。
void *pq_shift(pq *);

// （仅用于调试）获取内部 buffer 地址。
void **pq_get_buffer(pq *);

#endif