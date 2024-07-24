#ifndef MY_PKT
#define MY_PKT

struct pkt_impl;
typedef struct pkt_impl pkt;

struct serialze_ctx_impl;
typedef struct serialze_ctx_impl serialize_ctx;

struct parse_ctx_impl;
typedef struct parse_ctx_impl parse_ctx;

enum PktType { PktTyMsg = 0 };

enum PktHeaderField {
  PktFieldSender = 0,
  PktFieldReceiver = 1,
  PktFieldContentLength = 2,
};

#define MAX_HEADER_VALUE_SIZE ((0x1UL) << 10)
#define MAX_BODY_SIZE ((0x1UL) << 24)

enum ErrorReason {
  // 内存分配失败
  ErrAllocaFailed = 1,

  // 不支持的字段
  ErrNonSupportedField = 2,

  // 不支持的消息类型
  ErrNonSupportedMsgType = 3,

  // 值太大
  ErrSizeTooLarge = 4,

  // Buffer太小。有可能不能一次性放下要读取的字段的值，造成内存越界读取或者数据截断）。
  ErrTooSmallBuffer = 5,

  // 超过了 body size 极限大小，请减小写入的 chunk
  // 大小（或者考虑将消息拆分为多个 packet，todo：packet reassemble 待实现）
  // body 最大不能超过 MAX_BODY_SIZE
  ErrBodyTooLarge = 6
};

// 创建一个全新的 packet，使用该函数创建的 packet 用完后要通过 pkt_free
// 函数进行释放。
// 后续，packet 所有子资源的申请都通过 allocator 进行，通过 allocator_closure
// 提供可选的 closure 捕获。
// 可以使用 NULL 表示使用默认的 allocator 来负责 packet
// 本身以及子资源的资源分配。
// 当 allocator 为 NULL（或默认 allocator 时），allocator_closure 不会被提领。
// type 的取值详见 enum PktType.
int pkt_create(pkt **result, int type,
               void *(*alloca)(const int size, void *closure),
               void *alloca_closure, void (*deleter)(void *obj, void *payload),
               void *deleter_closure);

// 对 p 指向的 pkt 指针指向的 pkt 内存区域进行释放，释放后对 p 指向的 pkt 指针置
// NULL。
// 如果 packet（通过 pkt_create）创建时使用了自定义的（非默认的）
// allocator，那么，需要调用者提供匹配的 deleter。使用 NULL 作为 deleter 如果在
// pkt_create 中指定了使用默认 allocator。
// 当 deleter 为 NULL（或默认 deleter 时），deleter_closure 不会被提领。
void pkt_free(pkt **p);

// 对 key_idx 指代的 header 字段进行设置，值为 (buf, length) 指代的 blob。
// key_idx 的取值详见 enum PktHeaderField。
// 返回非零数表示失败（例如不支持的字段，或者值的格式不合法，或者 length
// 过大。） length 不得超过 MAX_HEADER_VALUE_SIZE.
int pkt_header_set_value(pkt *p, int key_idx, char *buf, int length);

// 读取 key_idx 指代的 header 字段的值，存放到（buf,
// legnth）区域中，返回值的实际 size。
// key_idx 的取值详见 enum PktHeaderField。
// length 表示 buf 支持存放的容量，不得小于 MAX_HEADER_VALUE_SIZE.
int pkt_header_get_value(pkt *p, int key_idx, char *buf, int length);

// 对 packet 的 body 进行写入
int pkt_body_send_chunk(pkt *p, char *buf, int length);

// 对 packet 的 body 进行读取，出错时返回负值，读取完毕返回
// 0，其它情况下返回得到的 chunk 的实际大小。每次读取不超过 length
// 字节。实际长度被写入 chunk_size。
int pkt_body_receive_chunk(pkt *p, char *buf, int length, int *chunk_size,
                           int offset);

// 把一个 packet 发送到 serialize context，出错时返回非 0 值。
int serialze_ctx_send_pkt(serialize_ctx *s_ctx, pkt *p);

// 从一个 serialize context 取出 chunk，出错时返回负值，返回 0 表示没有更多
// chunk（所有 chunk 已取出），返回正数表示 chunk 的大小。 chunk
// 的存放地址（buf）和最大写入大小（size）由 caller
// 提供（可以在栈上分配一个非常小的 buffer 用来存放 chunk，然后多次调用）。
int serialize_ctx_receive_chunk(serialize_ctx *s_ctx, char *buf, int size);

// 把一个 chunk 发送到 parse context，出错时返回负数（例如，chunk
// 超过大小限制或者格式错误），有结果可取出时，返回正数。
int parse_ctx_send_chunk(parse_ctx *p_ctx, char *buf, int size);

// 从 parse context 中取出一个 packet，返回 0 表示没有更多 packet 待取出，拿到的
// packet 用完后要调用 parse_ctx_free_pkt 函数进行释放。
int parse_ctx_receive_pkt(parse_ctx *p_ctx, pkt **p);

// 释放一个之前从 parse_ctx_receive_pkt 得到的 packet。
void parse_ctx_free_pkt(pkt *p);

#endif