#ifndef MY_LIMITATIONS
#define MY_LIMITATIONS

#define MAX_READ_BUF ((0x1UL) << 10)
#define MAX_WRITE_BUF_PER_CONN (((0x1UL) << 20) * 32)
#define MAX_SERVER_WRITE_BUF (((0x1UL) << 10) * 512)
#define MAX_RX_PACKETS_QUEUE 16
#define MAX_TX_PACKETS_QUEUE 16
#define MAX_READ_CHUNK_SIZE 128
#define MAX_NAME_LENGTH 32
#define MAX_PORT_STR_LEN 5
#define MAX_HOSTNAME_ALLOWED ((0x1UL) << 10)

#define MAX_HEADER_VALUE_SIZE ((0x1UL) << 10)
#define MAX_BODY_SIZE ((0x1UL) << 10)

// 一个 packet 至少需要能够容纳 magic words，若干个 header 字段，以及 body。
// 当 packet 解析器遇到一个太大的 packet 时，它会向调用者报告 ErrPacketTooBig
// 错误， 协议中的发送方收到 ErrPacketTooBig 错误后，需要对 packet 做
// segmentation。 MAX_PACKET_SIZE 和 MAX_BODY_SIZE
// 都具有约束作用，协议的参与者应当假设这两个限制中最严格的那个生效。
#define MAX_PACKET_SIZE ((0x1UL) << 12)

#endif