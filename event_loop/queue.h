#ifndef MY_QUEUE
#define MY_QUEUE

struct queue_impl;
typedef struct queue_impl queue;

// 创建一个 queue 对象
queue *queue_create(int max_len);

// 释放一个 queue 对象
void queue_free(queue **);

// 判断队列是否已满
int queue_is_fullfilled(queue *);

// 判断队列是否未满
int queue_has_space(queue *q);

// 把一个元素追加至队尾
void queue_enqueue(queue *q, void *item);

// 把一个元素从队列头部取出
void *queue_dequeue(queue *q);

// 获取队列当前元素数量
int queue_get_size(queue *q);

// 获取队列容量
int queue_get_capacity(queue *q);

#endif