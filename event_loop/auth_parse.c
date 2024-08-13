#include "auth_parse.h"

#include <ctype.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "limitations.h"

// 检查字符串 [base, base+len) 是否表示一个有效的 username.
// 返回非 0 值表示有效，返回 0 表示非有效。
int is_username_valid(char *base, int len) {
  char *end = &base[len];
  while (base < end) {
    if (!isalnum(*base++)) {
      return 0;
    }
  }
  return 1;
}

// 检查字符串 [base, base+len) 是否表示一个有效的 IPv4
// 地址字符串，输入字符串不应当包含除十进制阿拉伯数字和 '.' 之外的任何字符。
// 返回非 0 值表示有效，返回 0 值表示非有效。
int is_ipv4_str_valid(char *base, int len) {
  char *end = &base[len];
  int n_dots = 0;
  char addr_buf[INET_ADDRSTRLEN];
  int addr_head = 0;
  while (base < end) {
    if (*base == '.') {
      ++n_dots;
      if (n_dots > 3) {
        return 0;
      }

      ++base;
    } else if (isdigit(*base)) {
      while (base < end && isdigit(*base)) {
        if (addr_head >= 3) {
          return 0;
        }
        addr_buf[addr_head++] = *base++;
      }
      addr_buf[addr_head] = 0;

      char *end_ptr = NULL;
      long val = strtol(addr_buf, &end_ptr, 10);
      if (end_ptr != NULL && *end_ptr) {
        return 0;
      }

      if (val & (~(((1L) << 8) - 1))) {
        // val has bits set in non lower 8 bits.
        // in another word, we only allow bits set in lower 8 bits.
        return 0;
      }

      addr_head = 0;
    } else {
      return 0;
    }
  }

  return 1;
}

enum AF_VER {
  AFV_IPv4 = 4,
  AFV_IPv6 = 6,
};

enum IPStrSegType {
  // column seperator in ipv6 literals, e.g.: ':'
  COL = 1,

  // hex segment in ipv6 literals, e.g.: 'abcd' '192'
  OCTETS,

  // decimal, such as '192', '168', '1', '101'
  DEC,

  // wildcard mark in ipv6 literals, e.g.: '::'
  WILDCARD,

  // dot in ipv4 nested in ipv6 literals, e.g.: '.'
  DOT,
};

typedef struct ipstr_token_ {
  // See IPStrSegType
  int token_type;

  // buffer to store characters in this segment
  char buf[4];

  // number of characters that is already stored in the buffer
  int buflen;

} ipstr_token_t;

int is_hex(char c) {
  char hex_digits[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a',
                       'b', 'c', 'd', 'e', 'f', 'A', 'B', 'C', 'D', 'E', 'F'};

  for (int i = 0; i < sizeof(hex_digits); ++i) {
    if (hex_digits[i] == c) {
      return 1;
    }
  }

  return 0;
}

int is_decimal(char *buf, int len) {
  for (int i = 0; i < len; ++i) {
    if (!isdigit(buf[i])) {
      return 0;
    }
  }
  return 1;
}

void try_convert_octets_to_dec(ipstr_token_t *token) {
  if (token->token_type == OCTETS && is_decimal(token->buf, token->buflen)) {
    token->token_type = DEC;
  }
}

// 对 ipv6 string 进行 tokenize，结果写入 tokens，当成功时返回 tokens
// 个数，失败时返回 0。 base 指向被 tokenize 字符串的基地址，len 表示被 tokenize
// 字符串的长度（不包括末尾的 0）。
int tokenize_ipv6_str(ipstr_token_t *tokens, int max_n_segs, char *base,
                      int len) {
  int n_segs = 0;
  const int max_bufsize = sizeof(tokens[0].buf);
  char *head = base, *end = &base[len];
  while (head < end) {
    if (n_segs >= max_n_segs) {
      return 0;
    }

    if (head + 1 < end && strncmp(head, "::", 2) == 0) {
      memcpy(tokens[n_segs].buf, "::", 2);
      tokens[n_segs].buflen = 2;
      tokens[n_segs].token_type = WILDCARD;
      ++n_segs;
      head = &head[2];
    } else if (*head == ':') {
      tokens[n_segs].buf[0] = ':';
      tokens[n_segs].buflen = 1;
      tokens[n_segs].token_type = COL;
      ++n_segs;
      ++head;
    } else if (is_hex(*head)) {
      char *buf = tokens[n_segs].buf;
      int *buflen = &(tokens[n_segs].buflen);
      *buflen = 0;
      while (head < end && is_hex(*head) && *buflen < max_bufsize) {
        buf[*buflen] = *head;
        ++(*buflen);
        ++head;
      }
      tokens[n_segs].token_type = OCTETS;
      ++n_segs;
    } else if (*head == '.') {
      if (n_segs > 0) {
        try_convert_octets_to_dec(&tokens[n_segs - 1]);
      }

      tokens[n_segs].buf[0] = '.';
      tokens[n_segs].buflen = 1;
      tokens[n_segs].token_type = DOT;
      ++n_segs;
    } else {
      return 0;
    }
  }

  if (n_segs > 0) {
    try_convert_octets_to_dec(&tokens[n_segs - 1]);
  }

  return n_segs;
}

int test_pattern(int *pattern, int pattern_len, ipstr_token_t *tokens,
                 int n_tokens) {
  if (n_tokens != pattern_len) {
    return 0;
  }
  for (int i = 0; i < n_tokens; ++i) {
    if (pattern[i] != tokens[i].token_type) {
      return 0;
    }
  }
  return 1;
}

// 检查字符串 [base, base+len) 是否表示一个有效的 IPv6 地址，
// 返回非 0 值表示有效，返回 0 表示非有效。
int is_ipv6_str_valid(char *base, int len) {
  ipstr_token_t tokens[20];
  const int max_n_segs = sizeof(tokens) / sizeof(tokens[0]);
  int n_tokens = tokenize_ipv6_str(tokens, max_n_segs, base, len);
  if (n_tokens == 0) {
    return 0;
  }

  int n_octets = 0, n_cols = 0, n_wildcards = 0, n_decs = 0, n_dots = 0;
  for (int i = 0; i < n_tokens; ++i) {
    switch (tokens[i].token_type) {
      case OCTETS:
        ++n_octets;
        break;
      case COL:
        ++n_cols;
        break;
      case WILDCARD:
        ++n_wildcards;
        break;
      case DEC:
        ++n_decs;
        break;
      case DOT:
        ++n_dots;
        break;
      default:
        return 0;
    }
  }

  if (!(n_wildcards == 0 || n_wildcards == 1)) {
    return 0;
  }

  if (!(n_dots == 0 || n_dots == 3)) {
    return 0;
  }

  if (!(n_decs == 4 || n_decs == 0)) {
    return 0;
  }

  ipstr_token_t *head = tokens, *end = &tokens[n_tokens];
  if (head == end) {
    return 0;
  }

  while (head < end) {
    ipstr_token_t *next = &head[1];
    if (head->token_type == OCTETS) {
      if (next == end) {
        break;
      }

      if ((next->token_type == COL || next->token_type == WILDCARD)) {
        head = &head[2];
        continue;
      }

      return 0;
    } else if (head->token_type == DEC) {
      if (next == end) {
        break;
      }

      if (next->token_type == DOT) {
        head = &head[2];
        continue;
      }

      return 0;
    } else if (head == tokens && head->token_type == WILDCARD) {
      if (next == end) {
        return 1;
      } else if (next->token_type == OCTETS || next->token_type == DEC) {
        head = &head[1];
        continue;
      } else {
        return 0;
      }
    } else {
      return 0;
    }
  }

  if (n_wildcards == 0 && n_dots == 0) {
    return n_octets == 8;
  } else if (n_wildcards == 0 && n_dots == 3) {
    return n_octets == 6 && n_decs == 4;
  } else if (n_wildcards == 1 && n_dots == 0) {
    return n_octets <= 7;
  } else if (n_wildcards == 1 && n_dots == 3) {
    return n_octets <= 5;
  } else {
    return 0;
  }
}

int is_dns_label_valid(char *base, int len) {}

int is_port_str_valid(char *base, int len) {}

auth_parse_ctx *auth_parse_ctx_create() {
  auth_parse_ctx *ap_ctx = malloc(sizeof(auth_parse_ctx));
  ap_ctx->hostname = NULL;
  ap_ctx->username_len = 0;
  ap_ctx->port = NULL;
  ap_ctx->port_len = 0;
  ap_ctx->username = NULL;
  ap_ctx->username_len = 0;

  return ap_ctx;
}

int scan_chars(char *dst, char **src_start, char *src_end, char terminator,
               int max_reads) {
  if (src_start == NULL) {
    return 0;
  }

  char *head = *src_start;
  if (head == NULL) {
    return 0;
  }

  int n_reads = 0;
  while (head < src_end && *head != terminator && n_reads < max_reads) {
    dst[n_reads++] = *head++;
  }

  return n_reads;
}

void auth_parse_ctx_free(auth_parse_ctx **ap_ctx_ptr) {
  if (!ap_ctx_ptr) {
    return;
  }

  auth_parse_ctx *ap_ctx = *ap_ctx_ptr;
  if (!ap_ctx) {
    return;
  }

  if (ap_ctx->hostname) {
    free(ap_ctx->hostname);
  }

  if (ap_ctx->port) {
    free(ap_ctx->port);
  }

  if (ap_ctx->username) {
    free(ap_ctx->username);
  }

  free(ap_ctx);
  *ap_ctx_ptr = NULL;
}

int c_to_int(char c, long *result) {
  char buf[2];
  buf[0] = c;
  buf[1] = 0;
  char *end = NULL;
  *result = strtol(buf, &end, 10);
  if (end == buf) {
    return 1;
  }
  return 0;
}

// 解析形如 <username>@<host>:<port> 这样的 URI authority component.
// （见 RFC3986 section 3.2 "the authority component"）
// host 应当符合 RFC3986 section 3.2.2 约定的格式。
// 出错时返回非 0 值，调用者检查输入字符串是否符合相应的 RFC 规范。
//
// 参数说明：
// ctx: 保存解析结果
// origin: 输入字符串基址
// origin_len: 输入字符串长度 (不包括 null terminator)
// end_str: 用来保存 invalid 部分的起始地址
// last_state: 用来保存解析出错时 state machine 所处的状态
//
// 返回值和错误处理：
// 当解析成功时返回 0，返回非 0 值代表解析失败。
// 根据 enum ParseResultStatus（在 auth_parse.h 中定义）判断原因，在 *end_str
// 找到错误开始的地方。
enum ParseResultStatus auth_parse_ctx_do_parse(auth_parse_ctx *ctx,
                                               char *origin, int origin_len,
                                               char **end_str,
                                               int *last_state) {
  int state = NeedUsername;

  char *head = origin;
  char *end = &origin[origin_len];
  while (head < end && isspace(*head)) {
    ++head;
  }

  if (head >= end) {
    *end_str = head;
    return ErrUnexpectedTerminator;
  }

  int max_username = MAX_NAME_LENGTH;
  ctx->username = malloc(max_username);
  ctx->username_len = 0;

  int max_hostname = MAX_HOSTNAME_ALLOWED;
  ctx->hostname = malloc(max_hostname);
  ctx->hostname_len = 0;

  int max_portstr = MAX_PORT_STR_LEN;
  ctx->port = malloc(max_portstr);
  ctx->port_len = 0;

  while (head < end) {
    *end_str = head;
    *last_state = state;
    switch ((enum ParseAuthState)state) {
      case NeedUsername:

        ctx->username_len =
            scan_chars(ctx->username, &head, end, '@', max_username);

        if (head >= end) {
          *end_str = head;
          return ErrUnexpectedTerminator;
        }

        if (*head != '@') {
          return ErrUsernameTooLong;
        }

        if (!is_username_valid(ctx->username, ctx->username_len)) {
          return ErrInvalidUsername;
        }

        ++head;
        if (head >= end) {
          *end_str = head;
          return ErrUnexpectedTerminator;
        }

        state = NeedHost;
        continue;
      case NeedHost:

        if (*head == '[') {
          state = NeedIPv6Literal;
          ++head;
          if (head >= end) {
            *end_str = head;
            return ErrUnexpectedTerminator;
          }
        } else if (isdigit(*head)) {
          state = NeedIPv4Literal;
        } else if (isalpha(*head)) {
          // 我们不支持以数字开头的域名。对于 DNS label 和 subdomain
          // 的格式要求，我们遵循 RFC1035 指定的标准。
          // 并且我们也不会在解析 IPv4 地址失败后回退到解析 DNS label 的状态。
          state = NeedDNSLabel;
        } else {
          return ErrUnknownHostAddressFamily;
        }

        continue;
      case NeedIPv4Literal:
        ctx->hostname_len =
            scan_chars(ctx->hostname, &head, end, ':', max_hostname);

        if (head >= end) {
          *end_str = head;
          return ErrUnexpectedTerminator;
        }

        if (*head != ':') {
          return ErrIPv4LiteralLengthExceeded;
        }

        if (!is_ipv4_str_valid(ctx->hostname, ctx->hostname_len)) {
          return ErrInvalidIPv4Literal;
        }

        ++head;
        if (head >= end) {
          *end_str = head;
          return ErrUnexpectedTerminator;
        }

        state = NeedPort;
        continue;
      case NeedIPv6Literal:
        // See RFC4291 section 2.2 Text Representation of Addresses.

        ctx->hostname_len =
            scan_chars(ctx->hostname, &head, end, ']', max_hostname);

        if (head >= end) {
          *end_str = head;
          return ErrUnexpectedTerminator;
        }

        if (*head != ']') {
          return ErrIPv6LiteralLengthExceeded;
        }

        if (!is_ipv6_str_valid(ctx->hostname, ctx->hostname_len)) {
          return ErrInvalidIpv6Literal;
        }

        ++head;
        if (head >= end) {
          *end_str = head;
          return ErrUnexpectedTerminator;
        }

        if (*head != ':') {
          return ErrUnexpectedToken;
        }

        ++head;
        if (head >= end) {
          *end_str = head;
          return ErrUnexpectedTerminator;
        }

        state = NeedPort;
        continue;
      case NeedDNSLabel:
        ctx->hostname_len =
            scan_chars(ctx->hostname, &head, end, ':', max_hostname);

        if (head >= end) {
          *end_str = head;
          return ErrUnexpectedTerminator;
        }

        if (*head != ':') {
          return ErrHostnameLengthExceeded;
        }

        if (!is_dns_label_valid(head, ctx->hostname_len)) {
          return ErrInvalidHost;
        }

        ++head;
        if (head >= end) {
          *end_str = head;
          return ErrUnexpectedTerminator;
        }

        state = NeedPort;
        continue;
      case NeedPort:
        ctx->port_len = scan_chars(ctx->port, &head, end, 0, max_portstr);

        if (head < end) {
          *end_str = head;
          return ErrUnexpectedToken;
        }

        if (!is_port_str_valid(ctx->port, ctx->port_len)) {
          return ErrInvalidPort;
        }
    }
  }

  *end_str = head;

  return NoProblem;
}