#ifndef MYRINGBUF
#define MYRINGBUF

struct ringbuf_impl;
typedef struct ringbuf_impl ringbuf;

// 创建一个 ringbuf 对象，一个 ringbuf
// 是一个固定容量的、首尾相接的、「环形」的二进制数据存储区域。它理论上可以写入任意多的字节，但是当剩余容量不足时，最早写入的内容会被覆盖，并且
// size 最大增加至不超过它的 capacity。
ringbuf *ringbuf_create(int size);

// 释放一个 ringbuf 对象
void ringbuf_free(ringbuf *rb);

// 把一个 nbytes 大小的 chunk（基址由 src 表示）追加到 ringbuf 尾部。
int ringbuf_send_chunk(ringbuf *dst, const char *src, const int nbytes);

// 从 ringbuf 首部取出一个最大为 dst_bytes_max_writes bytes 的 chunk（基址由 dst
// 表示），返回 chunk 的实际大小。
int ringbuf_receive_chunk(char *dst, const int dst_bytes_max_writes,
                          ringbuf *src);

// 把一个（可能是之前取出但没用完的）chunk「返还」至 ringbuf 首部，相当于是
// ringbuf_receive_chunk 的逆操作。
void ringbuf_return_chunk(ringbuf *dst, const char *src, const int nbytes);

// 从 src 复制最多 len 字节大小的数据到 dst 尾部
int ringbuf_copy(ringbuf *dst, ringbuf *src, const int len);

// 从 src 转移最多 len 字节大小的数据到 dst 尾部
int ringbuf_transfer(ringbuf *dst, ringbuf *src, const int len);

// 清空一个 ringbuf 的所有内容，它的 size 会变成 0，又能容纳 capacity
// 个字节的数据。
void ringbuf_clear(ringbuf *rb);

// 判断一个 ringbuf 是否为空。
int ringbuf_is_empty(ringbuf *rb);

// 获取剩余容量
int ringbuf_get_remaining_capacity(ringbuf *rb);

// 当实际容量不及预期容量时进行扩容（i.e.
// 条件扩容），返回实际容量，如果扩容了，返回扩容后的实际容量（不一定等于
// expected_size，但一定不小于它）。
int ringbuf_upscale_if_needed(ringbuf **rb, const int expected_size);

// 获取 ringbuf 的容量（不是 size）
int ringbuf_get_capacity(ringbuf *rb);

#endif