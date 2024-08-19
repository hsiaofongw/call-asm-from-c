#ifndef MY_VALIDATE
#define MY_VALIDATE

// 检查字符串 [base, base+len) 是否表示一个有效的 username。
// 返回非 0 值表示有效，返回 0 表示非有效。
int is_username_valid(char *base, int len);

// 检查字符串 [base, base+len) 是否表示一个有效的 port string。
// 返回非 0 值表示它是有效的，返回 0 表示非有效。
int is_port_str_valid(char *base, int len);

// 检查字符串 [base, base+len) 是否表示一个有效的 IPv4
// 地址字符串，输入字符串不应当包含除十进制阿拉伯数字和 '.' 之外的任何字符。
// 返回非 0 值表示有效，返回 0 值表示非有效。
int is_ipv4_str_valid(char *base, int len);

// 检查字符串 [base, base+len) 是否表示一个有效的 IPv6 地址，
// 返回非 0 值表示有效，返回 0 表示非有效。
// An RFC4291 IPv6 text representation validator.
// Returns non-zero if the input string [base, base+len) is valid, otherwise
// returns 0.
int is_ipv6_str_valid(char *base, int len);

// 检查字符串 [base, base+len) 是否表示一个有效的 DNS domain name。
// See RFC section 2.3.1 "Preferred name syntax", and this implementation
// relaxed the requirements for a DNS label: a leading character in a DNS label
// could also be a digit but not just a alphabet.
// We do so because in real world many people have realdy use DNS domain name
// with all-digits label, for example `10086.cn` is not a RFC1035-compliant DNS
// domain name but it's actually in use.
int is_dns_name_valid(char *base, int len);

// 检查字符串 [base, base+len) 是否表示一个有效的 username.
// 返回非 0 值表示有效，返回 0 表示非有效。
int is_username_valid(char *base, int len);

#endif