#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

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

  fprintf(stderr, "Packet is sent to serialize_ctx.\n");

  fprintf(stderr, "Ready to receive: %d\n",
          serialize_ctx_is_ready_to_receive_chunk(s_ctx));

  offset = 0;
  while (1) {
    int chunk_size;
    status = serialize_ctx_receive_chunk(
        &sbuf[offset], MAX(0, sizeof(sbuf) - offset), &chunk_size, s_ctx);
    if (status != 0) {
      fprintf(stderr, "Failed to receive chunk from the serialize_ctx: %s\n",
              err_code_2_str(status));
      exit(1);
    }
    fprintf(stderr, "Received %d bytes chunk from the serialize_ctx.\n",
            chunk_size);
    if (chunk_size == 0) {
      fprintf(stderr, "All chunks extracted.\n");
      break;
    }
    offset += chunk_size;
  }

  int packet_len = offset;
  fprintf(stderr, "Dumping serialized blob (%d bytes) to file1...\n",
          packet_len);

  char filename1[] = "./file1.bin";
  int flags = 0;
  flags |= O_RDWR;
  flags |= O_CREAT;
  int modes = 0;
  modes |= S_IRUSR;
  modes |= S_IWUSR;

  int fd;
  fd = open(filename1, flags, modes);
  if (fd == -1) {
    fprintf(stderr, "Failed to open file %s: %s\n", filename1, strerror(errno));
    exit(1);
  }
  offset = 0;
  while (offset < packet_len) {
    int wbytes = write(fd, &sbuf[offset], MAX(0, packet_len - offset));
    if (wbytes < 0) {
      fprintf(stderr, "Failed to write: %s\n", strerror(errno));
      exit(1);
    }
    offset += wbytes;
  }

  parse_ctx *p_ctx;
  status = parse_ctx_create(&p_ctx, get_default_allocator());
  if (status != 0) {
    fprintf(stderr, "Failed to create parse_ctx: %s\n", err_code_2_str(status));
  }

  fprintf(stderr, "parse_ctx object is created at 0x%016lx\n",
          (unsigned long)p_ctx);
  fprintf(stderr, "parse_ctx is ready to send chunk: %d\n",
          parse_ctx_is_ready_to_send_chunk(p_ctx));

  offset = 0;
  while (1) {
    int size_accepted;
    int need_more;
    fprintf(stderr, "Chunk offset: %d\n", offset);
    int chunk_size = MIN(9, packet_len - offset);
    fprintf(stderr, "Chunk size: %d\n", chunk_size);
    status = parse_ctx_send_chunk(p_ctx, &sbuf[offset], chunk_size,
                                  &size_accepted, &need_more);
    int state = parse_ctx_get_state(p_ctx);
    fprintf(stderr, "State machine paused, current state: %s\n",
            parse_ctx_get_state_str(state));
    fprintf(stderr, "Accepted %d bytes\n", size_accepted);
    if (status == 0) {
      fprintf(stderr, "Parsing complete.\n");
      break;
    } else if (status == ErrNeedMore) {
      fprintf(stderr, "Need more: %d bytes\n", need_more);
      offset += size_accepted;
      continue;
    } else {
      fprintf(stderr, "Failed to parse: %s\n", err_code_2_str(status));
      exit(1);
    }
  }

  fprintf(stderr, "Is ready to extract parsed packet: %d\n",
          parse_ctx_is_ready_to_extract_packet(p_ctx));

  pkt *parsed_pkt;
  status = parse_ctx_receive_pkt(p_ctx, &parsed_pkt);
  if (status != 0) {
    fprintf(stderr, "Failed to extract: %s\n", err_code_2_str(status));
  }

  status = serialize_ctx_send_pkt(s_ctx, parsed_pkt);
  if (status != 0) {
    fprintf(stderr, "Failed to serialize parsed packet: %s\n",
            err_code_2_str(status));
  }

  offset = 0;
  while (1) {
    int chunk_size;
    status = serialize_ctx_receive_chunk(
        &sbuf[offset], MAX(0, sizeof(sbuf) - offset), &chunk_size, s_ctx);
    if (status != 0) {
      fprintf(stderr, "Failed to receive chunk from the serialize_ctx: %s\n",
              err_code_2_str(status));
      exit(1);
    }
    fprintf(stderr, "Received %d bytes chunk from the serialize_ctx.\n",
            chunk_size);
    if (chunk_size == 0) {
      fprintf(stderr, "All chunks extracted.\n");
      break;
    }
    offset += chunk_size;
  }

  packet_len = offset;
  fprintf(stderr, "Dumping serialized blob (%d bytes) to file2...\n",
          packet_len);

  char filename2[] = "./file2.bin";
  fd = open(filename2, flags, modes);
  if (fd == -1) {
    fprintf(stderr, "Failed to open file: %s: %s", filename2, strerror(errno));
    exit(1);
  }

  offset = 0;
  while (offset < packet_len) {
    int wbytes = write(fd, &sbuf[offset], MAX(0, packet_len - offset));
    if (wbytes < 0) {
      fprintf(stderr, "Failed to write: %s\n", strerror(errno));
      exit(1);
    }
    offset += wbytes;
  }

  parse_ctx_free(&p_ctx);
  serialize_ctx_free(s_ctx);
  pkt_free(&p);

  return 0;
}