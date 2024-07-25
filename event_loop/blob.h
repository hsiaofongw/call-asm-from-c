#include "err.h"

#ifndef MY_BLOB
#define MY_BLOB

struct blob_t {
  char *buf;
  int size;
  int capacity;
  struct alloc_t *mem;
};

struct blob_t *blob_create(int initial_capacity, struct alloc_t *allocator);

void blob_free(struct blob_t *b);

int blob_upscale_on_demand(struct blob_t *blob, int addend);

int blob_send_chunk(struct blob_t *b, char *buf, int length);

int blob_receive_chunk(char *dst, int dst_len, int *chunk_size,
                       struct blob_t *src, int src_offset);

// 预分配一个足够大小的空间以供写入，buf_out 拿到指针。
// 写入完成后，通过 blob_deem_buf_written 表达刚才写入了的量。
int blob_pre_allocate_buffer(struct blob_t *b, int requested_buf_size,
                             char **buf_out);

int blob_deem_buf_written(struct blob_t *b, int size);

#endif
