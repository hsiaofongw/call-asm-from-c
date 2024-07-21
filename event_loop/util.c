#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

void set_io_non_block(int fd) {
  int io_flags;
  io_flags = fcntl(fd, F_GETFL);
  if (io_flags == -1) {
    fprintf(stderr, "Can't read io flags of fd %d.\n", fd);
    exit(1);
  }

  io_flags = io_flags | O_NONBLOCK;
  if (fcntl(fd, F_SETFL, io_flags) == -1) {
    fprintf(stderr, "Failed to set io flags of fd %d.\n", fd);
    exit(1);
  }

  fprintf(stderr, "fd %d is now O_NONBLOCK\n", fd);
}

int cp_to_ring_buf(char *dst_base, int *dst_start_offset, int *dst_curr_size,
                   const int dst_capacity, const char *src, const int nbytes) {
  const int start_offset = *dst_start_offset;
  const int size0 = *dst_curr_size;
  for (int i = 0; i < nbytes; ++i) {
    dst_base[(start_offset + size0 + i) % dst_capacity] = src[i];
  }

  const int exceeded = size0 + nbytes - dst_capacity;
  *dst_curr_size += nbytes;
  if (exceeded > 0) {
    *dst_start_offset = (*dst_start_offset + exceeded) % dst_capacity;
    *dst_curr_size = dst_capacity;
    return exceeded;
  }
  return 0;
}

int get_chunk_from_ring_buf(char *dst, const int dst_bytes_max_writes,
                            char *ring_buf_base, int *ring_buf_offset,
                            int *ring_buf_curr_size,
                            const int ring_buf_capacity) {
  const int ring_size = *ring_buf_curr_size;
  if (ring_size == 0) {
    return 0;
  }

  int nbytes_written = 0;
  const int start_offset = *ring_buf_offset;
  while (nbytes_written < dst_bytes_max_writes && nbytes_written < ring_size) {
    dst[nbytes_written] =
        ring_buf_base[(start_offset + nbytes_written) % ring_buf_capacity];
    ++nbytes_written;
  }

  *ring_buf_offset = (start_offset + nbytes_written) % ring_buf_capacity;
  *ring_buf_curr_size -= nbytes_written;

  return nbytes_written;
}

void return_chunk_to_ring_buf(char *dst_base, int *dst_start_offset,
                              int *dst_curr_size, const int dst_capacity,
                              const char *src, const int nbytes) {
  *dst_start_offset =
      (*dst_start_offset + dst_capacity - nbytes) % dst_capacity;
  *dst_curr_size += nbytes;
  for (int i = 0; i < nbytes; ++i) {
    dst_base[(*dst_start_offset + i) % dst_capacity] = src[i];
  }
}