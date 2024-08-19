#ifndef MY_VALIDATE_DNS_NAME
#define MY_VALIDATE_DNS_NAME

// See RFC section 2.3.1 "Preferred name syntax", and this implementation
// relaxed the requirements for a DNS label: a leading character in a DNS label
// could also be a digit but not just a alphabet.
// We do so because in real world many people have realdy use DNS domain name
// with all-digits label, for example `10086.cn` is not a RFC1035-compliant DNS
// domain name but it's actually in use.
int is_dns_name_valid(char *base, int len);

#endif