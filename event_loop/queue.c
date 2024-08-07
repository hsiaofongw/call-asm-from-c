#include "queue.h"

#include <stdlib.h>
#include <string.h>

#include "ringbuf.h"

struct queue_impl {
  ringbuf *data;
};

queue *queue_create(int max_len) {
  struct queue_impl *q_ptr = malloc(sizeof(struct queue_impl));
  q_ptr->data = ringbuf_create(max_len * sizeof(void *));
  return q_ptr;
}

void queue_free(queue **q) {
  if (!q) {
    return;
  }

  queue *q_ptr = *q;
  if (!q_ptr) {
    return;
  }

  ringbuf_free(q_ptr->data);
  free(q_ptr);
  *q = NULL;
}

int queue_is_fullfilled(queue *q) {
  return ringbuf_get_remaining_capacity(q->data) == 0;
}

int queue_has_space(queue *q) {
  return ringbuf_get_remaining_capacity(q->data) > 0;
}

void queue_enqueue(queue *q, void *item) {
  ringbuf_send_chunk(q->data, (char *)&item, sizeof(void *));
}

void *queue_dequeue(queue *q) {
  char buf[sizeof(void *)];
  ringbuf_receive_chunk(buf, sizeof(buf), q->data);
  void *result;
  memcpy(&result, buf, sizeof(buf));
  return result;
}

int queue_get_size(queue *q) {
  return ringbuf_get_size(q->data) / sizeof(void *);
}

int queue_get_capacity(queue *q) {
  return ringbuf_get_capacity(q->data) / sizeof(void *);
}

int queue_transfer(queue *dst, queue *src) {
  int n_items = 0;
  while (queue_has_space(dst) && queue_get_size(src) > 0) {
    void *elem = queue_dequeue(src);
    queue_enqueue(dst, elem);
  }
  return n_items;
}
