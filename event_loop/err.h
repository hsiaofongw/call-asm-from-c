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
  ErrSerializeCtxBusy = 7,

  // 容量不足
  ErrNoEnoughCapacity = 8,

  // 封包太大
  ErrPacketTooBig = 9,

  // MagicWords 不匹配
  ErrMagicWordsMisMatch = 10,

  // 没有数据可用于解析，请确保提供的 chunk buffer 非空（或者 parse context
  // 内部的 buffer 非空）
  ErrNoDataToParse = 11,

  // 需要更多数据
  ErrNeedMore = 12,

  // Header 字段值不合法，例如太长了
  ErrInvalidHeaderValue = 13,

  // 请先取出已解析的 packet
  ErrExtractParsedPacketFirst = 14,

  // 解析未完成
  ErrParsingIsIncomplete = 15,
};

// 获取错误代码对应的 null-terminated 描述字符串。
char *err_code_2_str(enum ErrorReason code);

#endif