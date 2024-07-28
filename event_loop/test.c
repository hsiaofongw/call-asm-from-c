#include <stdio.h>
#include <string.h>

#include "err.h"
#include "pkt.h"

int main() {
  pkt *p;
  int status = pkt_create(&p, PktTyMsg, get_default_allocator());
  if (status != 0) {
    fprintf(stderr, "Failed to create packet: %s\n", err_code_2_str(status));
    exit(1);
  }

  char sbuf[PAGE_SIZE];
  fprintf(stderr, "Type: %s\n", pkt_type_2_str(pkt_get_type(p)));

  char sender[] = "alice";
  char receiver[] = "bob";
  status = pkt_header_set_value(p, PktFieldSender, sender, sizeof(sender));
  if (status != 0) {
    fprintf(stderr, "Failed to set sender: %s\n", err_code_2_str(status));
  }

  status =
      pkt_header_set_value(p, PktFieldReceiver, receiver, sizeof(receiver));
  if (status != 0) {
    fprintf(stderr, "Failed to set receiver: %s\n", err_code_2_str(status));
  }

  int sender_size, receiver_size;
  status =
      pkt_header_get_value(p, PktFieldSender, sbuf, sizeof(sbuf), &sender_size);
  if (status != 0) {
    fprintf(stderr, "Failed to get sender: %s\n", err_code_2_str(status));
  }
  sbuf[sender_size] = 0;
  fprintf(stderr, "Sender: %s\n", sbuf);

  status = pkt_header_get_value(p, PktFieldReceiver, sbuf, sizeof(sbuf),
                                &receiver_size);
  if (status != 0) {
    fprintf(stderr, "Failed to get receiver: %s\n", err_code_2_str(status));
  }
  sbuf[receiver_size] = 0;
  fprintf(stderr, "Receiver: %s\n", sbuf);

  char msg[] = "abcdef";
  status = pkt_body_send_chunk(p, msg, sizeof(msg));
  if (status != 0) {
    fprintf(stderr, "Failed to send chunk to packet: %s\n",
            err_code_2_str(status));
  }

  fprintf(stderr, "Message wrote: %s\n", msg);

  int content_len_var_size;
  status = pkt_header_get_value(p, PktFieldContentLength, sbuf, sizeof(sbuf),
                                &content_len_var_size);
  if (status != 0) {
    fprintf(stderr, "Failed to get content-length: %s\n",
            err_code_2_str(status));
  }
  int received_content_len;
  if (content_len_var_size > sizeof(received_content_len)) {
    fprintf(stderr, "Incorrect content-length var size: %d\n",
            content_len_var_size);
  }
  memcpy(&received_content_len, sbuf, content_len_var_size);
  fprintf(stderr, "Received content-length: %d\n", received_content_len);

  int offset = 0;
  while (1) {
    int chunk_size;
    status = pkt_body_receive_chunk(p, sbuf, 4, &chunk_size, offset);
    if (status != 0) {
      fprintf(stderr, "Failed to receive chunk: %s\n", err_code_2_str(status));
      break;
    }
    offset += chunk_size;
    if (chunk_size >= 0 && chunk_size < sizeof(sbuf)) {
      sbuf[chunk_size] = 0;
    }
    if (chunk_size >= sizeof(sbuf)) {
      chunk_size = sizeof(sbuf) - 1;
    }
    if (chunk_size == 0) {
      fprintf(stderr, "EOF\n");
      break;
    }
    fprintf(stderr, "Got %d bytes message chunk: %s\n", chunk_size, sbuf);
  }

  serialize_ctx *s_ctx = serialize_ctx_create(get_default_allocator());
  if (!s_ctx) {
    fprintf(stderr, "Failed to create serialize context.\n");
    exit(1);
  }

  status = serialize_ctx_send_pkt(s_ctx, p);
  if (status != 0) {
    fprintf(stderr, "Failed to serialize: %s\n", err_code_2_str(status));
    exit(1);
  }

  serialize_ctx_free(s_ctx);

  pkt_free(&p);

  return 0;
}