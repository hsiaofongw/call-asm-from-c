#ifndef MY_AUTH_PARSE
#define MY_AUTH_PARSE

typedef struct auth_parse_ctx {
  char *username;
  int username_len;

  char *hostname;
  int hostname_len;

  char *port;
  int port_len;

} auth_parse_ctx;

#endif