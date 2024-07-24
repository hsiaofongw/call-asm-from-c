#ifndef MY_ERR
#define MY_ERR

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
  ErrBodyTooLarge = 6,

  // 请先清空 serialize_ctx（例如通过反复调用 receive chunk）
  ErrSerializeCtxBusy = 7
};

#endif