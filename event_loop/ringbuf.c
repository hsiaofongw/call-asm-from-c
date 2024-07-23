#include "ringbuf.h"

#include <stdlib.h>

struct ringbuf_impl {
  char *buf;
  int start_offset;
  int size;
  int capacity;
};

struct ringbuf_impl *ringbuf_create(int size) {
  struct ringbuf_impl *c = malloc(sizeof(struct ringbuf_impl));
  c->buf = malloc(size);
  c->start_offset = 0;
  c->size = 0;
  c->capacity = size;
  return c;
}

void ringbuf_free(struct ringbuf_impl *c) {
  free(c->buf);
  free(c);
}

int ringbuf_send_chunk(struct ringbuf_impl *dst, const char *src,
                       const int nbytes) {
  const int start_offset = dst->start_offset;
  const int size0 = dst->size;
  for (int i = 0; i < nbytes; ++i) {
    dst->buf[(start_offset + size0 + i) % dst->capacity] = src[i];
  }

  dst->size += nbytes;
  const int exceeded = dst->size - dst->capacity;
  if (exceeded > 0) {
    dst->start_offset = (dst->start_offset + exceeded) % dst->capacity;
    dst->size = dst->capacity;
    return exceeded;
  }
  return 0;
}

int ringbuf_receive_chunk(char *dst, const int dst_bytes_max_writes,
                          struct ringbuf_impl *src) {
  const int ring_size = src->size;
  if (ring_size == 0) {
    return 0;
  }

  int nbytes_written = 0;
  const int start_offset = src->start_offset;
  while (nbytes_written < dst_bytes_max_writes && nbytes_written < ring_size) {
    dst[nbytes_written] =
        src->buf[(start_offset + nbytes_written) % src->capacity];
    ++nbytes_written;
  }

  src->start_offset = (start_offset + nbytes_written) % src->capacity;
  src->size -= nbytes_written;

  return nbytes_written;
}

void ringbuf_return_chunk(struct ringbuf_impl *dst, const char *src,
                          const int nbytes) {
  dst->start_offset =
      (dst->start_offset + dst->capacity - nbytes) % dst->capacity;
  dst->size += nbytes;
  for (int i = 0; i < nbytes; ++i) {
    dst->buf[(dst->start_offset + i) % dst->capacity] = src[i];
  }
}

int ringbuf_copy(struct ringbuf_impl *dst, struct ringbuf_impl *src,
                 const int len) {
  int actual_writes = 0;
  for (int i = 0; i < src->size && i < len; ++i) {
    dst->buf[(dst->start_offset + dst->size + i) % dst->capacity] =
        src->buf[(src->start_offset + i) % src->capacity];
    ++actual_writes;
  }
  dst->size += actual_writes;
  if (dst->size > dst->capacity) {
    int exceeded = dst->size - dst->capacity;
    dst->size = dst->capacity;
    dst->start_offset = (dst->start_offset + exceeded) % dst->capacity;
  }
  return actual_writes;
}

int ringbuf_transfer(struct ringbuf_impl *dst, struct ringbuf_impl *src,
                     const int len) {
  int actual_writes = 0;
  for (int i = 0; i < src->size && i < len; ++i) {
    dst->buf[(dst->start_offset + dst->size + i) % dst->capacity] =
        src->buf[(src->start_offset + i) % src->capacity];
    ++actual_writes;
  }

  dst->size += actual_writes;
  if (dst->size > dst->capacity) {
    int exceeded = dst->size - dst->capacity;
    dst->size = dst->capacity;
    dst->start_offset = (dst->start_offset + exceeded) % dst->capacity;
  }

  src->size -= actual_writes;
  src->start_offset = (src->start_offset + actual_writes) % src->capacity;

  return actual_writes;
}

void ringbuf_clear(struct ringbuf_impl *rb) { rb->size = 0; }

int ringbuf_is_empty(struct ringbuf_impl *rb) { return rb->size == 0 ? 1 : 0; }

int ringbuf_get_remaining_capacity(ringbuf *rb) {
  return rb->capacity - rb->size;
}

int ringbuf_upscale_if_needed(struct ringbuf_impl **rb,
                              const int expected_size) {
  struct ringbuf_impl *src = *rb;
  if (src->size >= expected_size) {
    return src->size;
  }

  struct ringbuf_impl *new_rb = ringbuf_create(expected_size);
  for (int i = 0; i < src->size; ++i) {
    new_rb->buf[i] = src->buf[(src->start_offset + i) % src->capacity];
  }
  *rb = new_rb;
  return new_rb->size;
}

int ringbuf_get_capacity(struct ringbuf_impl *rb) { return rb->capacity; }