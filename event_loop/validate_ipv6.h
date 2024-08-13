#ifndef MY_VALIDATE_IPV6
#define MY_VALIDATE_IPV6

// 检查字符串 [base, base+len) 是否表示一个有效的 IPv6 地址，
// 返回非 0 值表示有效，返回 0 表示非有效。
int is_ipv6_str_valid(char *base, int len);

#endif