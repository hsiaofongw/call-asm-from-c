#include "auth_parse.h"

#include <ctype.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "limitations.h"

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

enum ParseAuthState {
  NeedUsername,
  NeedHost,
  NeedIPv4Literal,
  NeedIPv6Literal,
  NeedDNSLabel,
  NeedPort
};

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
int auth_parse_ctx_do_parse(auth_parse_ctx *ctx, char *origin, int origin_len) {
  char *head = origin;
  char *end = &origin[origin_len];

  int state = NeedUsername;
  int ipv4_buf[4] = {0, 0, 0, 0};
  int v4_head = 0;

  while (head < end) {
    char c = *head++;
    if (isspace(c)) {
      continue;
    }

    switch ((enum ParseAuthState)state) {
      case NeedUsername:
        if (c == '@') {
          state = NeedHost;
          continue;
        }

        if (!isalnum(c)) {
          return 1;
        }

        if (ctx->username == NULL) {
          ctx->username = malloc(MAX_NAME_LENGTH * sizeof(char));
        }

        if (ctx->username_len >= MAX_NAME_LENGTH) {
          return 1;
        }

        ctx->username[ctx->username_len++] = c;
        continue;
      case NeedHost:
        if (c == '[') {
          state = NeedIPv6Literal;
        } else if (isdigit(c)) {
          state = NeedIPv4Literal;
          --head;
        } else if (isalpha(c)) {
          // 我们不支持以数字开头的域名。对于 DNS label 和 subdomain
          // 的格式要求，我们遵循 RFC1035 指定的标准。
          // 并且我们也不会在解析 IPv4 地址失败后回退到解析 DNS label 的状态。
          state = NeedDNSLabel;
          --head;
        } else {
          return 1;
        }

        continue;
      case NeedIPv4Literal:
        if (c == '.') {
          ++v4_head;
          if (v4_head >= 4) {
            return 1;
          }
        } else if (isdigit(c)) {
          long digit_value;
          if (c_to_int(c, &digit_value) != 0) {
            return 1;
          }

          // check if any bits except the lower 8 bits are set.
          int lower_8_masks = (1 << 8) - 1;
          if (digit_value & (~lower_8_masks)) {
            return 1;
          }

          ipv4_buf[v4_head] = ipv4_buf[v4_head] * 10 + ((int)digit_value);
          if (ipv4_buf[v4_head] > 255) {
            return 1;
          }
        } else if (c == ':') {
          state = NeedPort;
          continue;
        } else {
          return 1;
        }

        if (ctx->hostname == NULL) {
          ctx->hostname = malloc(INET_ADDRSTRLEN);
        }
        ctx->hostname[ctx->hostname_len++] = c;
        continue;
      case NeedIPv6Literal:
        // See RFC4291 section 2.2 Text Representation of Addresses.
        char *v6_start = head;

        while (head < end && *head != ']') {
          ++head;
        }

        if (head >= end) {
          return 1;
        }

        ctx->hostname_len = (unsigned long)(head - v6_start);
        if (ctx->hostname != NULL) {
          return 1;
        }

        ctx->hostname = malloc(ctx->hostname_len * sizeof(char));
        memcpy(ctx->hostname, v6_start, ctx->hostname_len);

        ++head;
        if (head >= end || *head != ':') {
          return 1;
        }
        state = NeedPort;
        continue;
      case NeedDNSLabel:
        if (c == '.') {
          if (ctx->hostname == NULL || ctx->hostname_len == 0) {
            // empty label
            return 1;
          } else if (ctx->hostname[ctx->hostname_len - 1] == '.') {
            // two consective '.'s.
            return 1;
          }
        } else if (c == '-' || isalnum(c)) {
          if (isdigit(c) || c == '-') {
            if (ctx->hostname == NULL || ctx->hostname_len == 0 ||
                ctx->hostname[ctx->hostname_len - 1] == '.') {
              return 1;
            }
          }
        } else if (c == ':') {
          if (ctx->hostname == NULL || ctx->hostname_len == 0) {
            return 1;
          }

          state = NeedPort;
          continue;
        } else {
          return 1;
        }

        if (ctx->hostname == NULL) {
          ctx->hostname = mallco(MAX_HEADER_VALUE_SIZE);
        }

        if (ctx->hostname_len >= MAX_HEADER_VALUE_SIZE) {
          return 1;
        }

        ctx->hostname[ctx->hostname_len++] = c;
        continue;
    }
  }
}