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

// 把 src ringbuf 的内容全部移至（追加至）dst ringbuf（的尾部），操作完成之后
// src ringbuf 为空（size 变为 0），而 dst ringbuf 的 size
// 可能会增加（如果它处于未满状态），如果 dst ringbuf 在这样操作之间已满，则其
// size 不会增加，但是先前（最早些时候）写入的内容会被覆盖。
int ringbuf_transfer_all(ringbuf *dst, ringbuf *src);

// 类似于 ringbuf_transfer_all，但是 src ringbuf 的内容会被保留而非清空。
int ringbuf_copy_all(ringbuf *dst, ringbuf *src);

// 清空一个 ringbuf 的所有内容，它的 size 会变成 0，又能容纳 capacity
// 个字节的数据。
void ringbuf_clear(ringbuf *rb);

// 判断一个 ringbuf 是否为空。
int ringbuf_is_empty(ringbuf *rb);

#endif