#include "alloc.h"

#ifndef MY_PKT
#define MY_PKT

struct pkt_impl;
typedef struct pkt_impl pkt;

struct serialize_ctx_impl;
typedef struct serialize_ctx_impl serialize_ctx;

struct parse_ctx_impl;
typedef struct parse_ctx_impl parse_ctx;

enum PktType { PktTyMsg = 0 };

enum PktHeaderField {
  PktFieldSender = 0,
  PktFieldReceiver = 1,
  PktFieldContentLength = 2,
};

#define MAX_HEADER_VALUE_SIZE ((0x1UL) << 10)
#define MAX_BODY_SIZE ((0x1UL) << 10)

// 一个 packet 至少需要能够容纳 magic words，若干个 header 字段，以及 body。
// 当 packet 解析器遇到一个太大的 packet 时，它会向调用者报告 ErrPacketTooBig
// 错误， 协议中的发送方收到 ErrPacketTooBig 错误后，需要对 packet 做
// segmentation。 MAX_PACKET_SIZE 和 MAX_BODY_SIZE
// 都具有约束作用，协议的参与者应当假设这两个限制中最严格的那个生效。
#define MAX_PACKET_SIZE ((0x1UL) << 12)

// 创建一个全新的 packet，使用该函数创建的 packet 用完后要通过 pkt_free
// 函数进行释放。
// 后续，packet 所有子资源的申请都通过 allocator 进行，通过 allocator_closure
// 提供可选的 closure 捕获。
// 可以使用 NULL 表示使用默认的 allocator 来负责 packet
// 本身以及子资源的资源分配。
// 当 allocator 为 NULL（或默认 allocator 时），allocator_closure 不会被提领。
// type 的取值详见 enum PktType.
int pkt_create(pkt **result, int type, struct alloc_t *allocator);

// 对 p 指向的 pkt 指针指向的 pkt 内存区域进行释放，释放后对 p 指向的 pkt 指针置
// NULL。
// 如果 packet（通过 pkt_create）创建时使用了自定义的（非默认的）
// allocator，那么，需要调用者提供匹配的 deleter。使用 NULL 作为 deleter 如果在
// pkt_create 中指定了使用默认 allocator。
// 当 deleter 为 NULL（或默认 deleter 时），deleter_closure 不会被提领。
void pkt_free(pkt **p);

int pkt_set_type(pkt *p, int type);
int pkt_get_type(pkt *);

char *pkt_type_2_str(int type);

// 对 key_idx 指代的 header 字段进行设置，值为 (buf, length) 指代的 blob。
// key_idx 的取值详见 enum PktHeaderField。
// 返回非零数表示失败（例如不支持的字段，或者值的格式不合法，或者 length
// 过大。） length 不得超过 MAX_HEADER_VALUE_SIZE.
int pkt_header_set_value(pkt *p, int key_idx, char *buf, int length);

// 读取 key_idx 指代的 header 字段的值，存放到（buf,
// legnth）区域中，返回值的实际 size。
// key_idx 的取值详见 enum PktHeaderField。
// length 表示 buf 支持存放的容量，不得小于 MAX_HEADER_VALUE_SIZE.
int pkt_header_get_value(pkt *p, int key_idx, char *buf, int length, int *size);

// 对 packet 的 body 进行写入
int pkt_body_send_chunk(pkt *p, char *buf, int length);

// 对 packet 的 body 进行读取，出错时返回负值，读取完毕返回
// 0，其它情况下返回得到的 chunk 的实际大小。每次读取不超过 length
// 字节。实际长度被写入 chunk_size。
int pkt_body_receive_chunk(pkt *p, char *buf, int length, int *chunk_size,
                           int offset);

// 创建一个 serialize_ctx 对象
serialize_ctx *serialize_ctx_create(struct alloc_t *allocator);

// 把一个 packet 发送到 serialize_ctx。
// 在通过调用 serialize_ctx_receive_chunk 把全部 chunk
// 取出来之前，你不能再次调用 serialize_ctx_send_pkt.
int serialize_ctx_send_pkt(serialize_ctx *s_ctx, pkt *p);

// 从一个 serialize_ctx 取出 chunk，出错时返回非零值。通过 *chunk_size
// 取得实际写入 dst 的 chunk 大小，当 *chunk_size == 0 时，可以认为
// serialize_ctx 内的 chunk 已经全部取出（意味着可以再次调用
// serialize_ctx_send_pkt）
int serialize_ctx_receive_chunk(char *dst, int max_len, int *chunk_size,
                                serialize_ctx *src);

// 判断是否可以调用 serialize_ctx_send_pkt「发送」packet 给 serialize_ctx。
int serialize_ctx_is_ready_to_send_pkt(serialize_ctx *);

// 判断是否可以调用 serialize_ctx_receive_chunk 从 serialize_ctx
// 中「接收」chunk。
int serialize_ctx_is_ready_to_receive_chunk(serialize_ctx *);

// 释放一个 serialize_ctx 对象
void serialize_ctx_free(serialize_ctx *);

// 创建一个 parse_ctx 对象
int parse_ctx_create(parse_ctx **p_ctx, struct alloc_t *allocator);

// 释放一个 parse_ctx 对象，
// 注意：通过该 parse_ctx 对象解析出的 packet 不会被释放，需要手动释放。
void parse_ctx_free(parse_ctx **);

// 把一个 chunk 发送到一个 parse_ctx 对象，通过 size_accept 指针写入接收了的
// chunk 的 size，并不是多大的 chunk 都会照单全收。
// 出错时，返回非 0 值。
int parse_ctx_send_chunk(parse_ctx *p_ctx, char *buf, int size,
                         int *size_accepted, int *need_more);

// 从一个 parse_ctx 对象中取出一个 packet，
// packet 用完后要调用 pkt_free 函数进行释放。
int parse_ctx_receive_pkt(parse_ctx *p_ctx, pkt **p);

// 判断是否可以将 chunk 发送给 parse_ctx
int parse_ctx_is_ready_to_send_chunk(parse_ctx *);

// 判断是否可以从 parse_ctx 取出 packet
int parse_ctx_is_ready_to_extract_packet(parse_ctx *);

// 获取状态机当前状态
int parse_ctx_get_state(parse_ctx *);

// 把状态机状态转换为 null-terminated 字符串
char *parse_ctx_get_state_str(int state);

#endif