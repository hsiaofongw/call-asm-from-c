#include "blob.h"

#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include "alloc.h"

struct blob_t *blob_create(int initial_capacity, struct alloc_t *allocator) {
  struct blob_t *b =
      allocator->alloc(sizeof(struct blob_t), allocator->closure);
  b->buf = allocator->alloc(initial_capacity, allocator->closure);
  b->capacity = initial_capacity;
  b->mem = allocator;
  b->size = 0;
  return b;
}

void blob_free(struct blob_t *b) {
  void (*deleter)(void *addr, void *closure) = b->mem->deleter;
  void *closure = b->mem->closure;
  deleter(b->buf, closure);
  deleter(b, closure);
}

int blob_upscale_on_demand(struct blob_t *blob, int addend) {
  if (blob->size + addend <= blob->capacity) {
    return 0;
  }

  int new_capacity = align_default(blob->size + addend);
  char *new_buf = blob->mem->alloc(new_capacity, blob->mem->closure);
  if (!new_buf) {
    return ErrAllocaFailed;
  }

  if (blob->size > 0 && blob->buf != NULL) {
    memcpy(new_buf, blob->buf, blob->size);
    blob->mem->deleter(blob->buf, blob->mem->closure);
  }
  blob->buf = new_buf;
  blob->capacity = new_capacity;
  return 0;
}

int blob_send_chunk(struct blob_t *b, char *buf, int length) {
  int status = blob_upscale_on_demand(b, length);
  if (status != 0) {
    return status;
  }

  memcpy(&b->buf[b->size], buf, length);
  b->size += length;
  return 0;
}

int blob_receive_chunk(char *dst, int dst_len, int *chunk_size,
                       struct blob_t *src, int src_offset) {
  int remain_src_size = src->size - src_offset;
  *chunk_size = MAX(MIN(dst_len, remain_src_size), 0);
  if (*chunk_size == 0) {
    return 0;
  }

  memcpy(dst, &src->buf[src_offset], *chunk_size);

  return 0;
}

int blob_pre_allocate_buffer(struct blob_t *b, int requested_buf_size,
                             char **buf_out) {
  int status = blob_upscale_on_demand(b, requested_buf_size);
  if (status != 0) {
    return status;
  }

  *buf_out = &b->buf[b->size];

  return 0;
}

int blob_deem_buf_written(struct blob_t *b, int size) {
  b->size += size;
  if (b->size > b->capacity) {
    b->size = b->capacity;
    return ErrNoEnoughCapacity;
  }
  return 0;
}

int blob_get_size(struct blob_t *b) { return b->size; }

void blob_clear(struct blob_t *b) { b->size = 0; }