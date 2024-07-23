#ifndef MY_UTIL
#define MY_UTIL

// 对一个 fd 指代的文件设定 O_NONBLOCK 操作模式。
void set_io_non_block(int fd);

// 将 nbytes bytes 大小的数据从 src 拷贝到 dst（一个 ring buffer），如果 dst
// 剩余空间不足以存储 nbytes 这么多字节的数据， 那么 dst
// 早些时候存进去的数据将会被覆写。
// 返回覆写了多少字节，如果没有数据被覆写，返回 0。
int cp_to_ring_buf(char *dst_base, int *dst_start_offset, int *dst_curr_size,
                   const int dst_capacity, const char *src, const int nbytes);

// 从 ring buffer 中取出一个 chunk，向 dst 写入不超过 dst_bytes_max_writes bytes
// 大小的数据，返回实际写入的长度（也就是 chunk 的实际大小），当返回 0
// 时，可以认为 ring buffer 已经为空。
int get_chunk_from_ring_buf(char *dst, const int dst_bytes_max_writes,
                            char *ring_buf_base, int *ring_buf_offset,
                            int *ring_buf_curr_size,
                            const int ring_buf_capacity);

// 把一个 chunk 返还给 ring buffer，这个 chunk 的大小不必和之前
// get_chunk_from_ring_buf 取出的 chunk 的大小相等， 常用于取出了一个 chunk
// 太大一次性用不完的情况，这时如果直接把没用完的那部分 chunk 丢弃会不妥。
// ring buffer 中字节的顺序得到保持，例如：
//
// 调用 get_chunk_from_ring_buf 之前，ring buffer 中的内容为：
//
// a b c d e
//
// 调用 get_chunk_from_ring_buf 取出了 3 bytes 大小的 chunk [a b c]，那么 ring
// buffer 剩下：
//
// d e
//
// 此时假如把 [b c] 返还给 ring buffer，返还之后 ring buffer 中的内容为：
//
// b c d e
void return_chunk_to_ring_buf(char *dst_base, int *dst_start_offset,
                              int *dst_curr_size, const int dst_capacity,
                              const char *src, const int nbytes);

int get_peer_pretty_name(char *buf, ssize_t buflen, struct sockaddr *addr);

void sprint_conn(char *buf, size_t buflen, int fd);

#endif