#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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

int get_peer_pretty_name(char *buf, ssize_t buflen, struct sockaddr *addr) {
  int ip_str_len = 0;
  int portnum = 0;
  if (addr->sa_family == AF_INET6) {
    struct sockaddr_in6 *s = (struct sockaddr_in6 *)addr;
    char ipv6_addr_buf[INET6_ADDRSTRLEN + 1];
    if (inet_ntop(AF_INET6, &s->sin6_addr, ipv6_addr_buf, INET6_ADDRSTRLEN) ==
        NULL) {
      fprintf(stderr, "Failed to stringify IPv6 address: %s\n",
              strerror(errno));
      return -1;
    }

    return snprintf(buf, buflen, "[%s]:%d", ipv6_addr_buf, (s->sin6_port));
  } else if (addr->sa_family == AF_INET) {
    struct sockaddr_in *s = (struct sockaddr_in *)addr;
    char ipv4_addr_buf[INET_ADDRSTRLEN + 1];
    if (inet_ntop(AF_INET, &s->sin_addr.s_addr, ipv4_addr_buf,
                  INET_ADDRSTRLEN) == NULL) {
      fprintf(stderr, "Failed to stringify IP address: %s\n", strerror(errno));
      return -1;
    }

    return snprintf(buf, buflen, "%s:%d", ipv4_addr_buf, ntohs(s->sin_port));
  } else {
    fprintf(stderr, "Unknown address family.\n");
    return -1;
  }
}

void sprint_conn(char *buf, size_t buflen, int fd) {
  if (fd == STDIN_FILENO) {
    snprintf(buf, buflen, "[stdin]");
  } else if (fd == STDOUT_FILENO) {
    snprintf(buf, buflen, "[stdout]");
  } else if (fd == STDERR_FILENO) {
    snprintf(buf, buflen, "[stderr]");
  } else {
    struct sockaddr_storage cli_addr_store;
    socklen_t cli_addr_size = sizeof(cli_addr_size);

    if (getpeername(fd, (struct sockaddr *)&cli_addr_store, &cli_addr_size) ==
        -1) {
      fprintf(stderr, "Failed to get peer address (fd = %d), errorno: %s\n", fd,
              strerror(errno));
    }

    if (get_peer_pretty_name(buf, buflen,
                             (struct sockaddr *)(&cli_addr_store)) == -1) {
      fprintf(stderr, "Failed to stringify peer address.\n");
    }
  }
}
