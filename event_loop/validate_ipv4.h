
#ifndef MY_VALIDATE_IPV4
#define MY_VALIDATE_IPV4

// 检查字符串 [base, base+len) 是否表示一个有效的 IPv4
// 地址字符串，输入字符串不应当包含除十进制阿拉伯数字和 '.' 之外的任何字符。
// 返回非 0 值表示有效，返回 0 值表示非有效。
int is_ipv4_str_valid(char *base, int len);

#endif