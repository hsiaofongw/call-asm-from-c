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

#include "conn_manage.h"

#define MAX_PEER_NAME 256
char peer_name_buf[MAX_PEER_NAME];

#define MAX_READ_BUFFER 1024
char read_buf[MAX_READ_BUFFER];

struct conn_ctx {
  int fd;
};

#define FD_CAPACITY_PER_INT (sizeof(int) * 8)
int read_fdset_storage[FD_SETSIZE / FD_CAPACITY_PER_INT];
int write_fdset_storage[FD_SETSIZE / FD_CAPACITY_PER_INT];

fd_set *read_interest = (void *)read_fdset_storage;
fd_set *write_interest = (void *)write_fdset_storage;

void close_fd_or_panic(int fd) {
  if (FD_ISSET(fd, read_interest)) {
    FD_CLR(fd, read_interest);
  }

  if (FD_ISSET(fd, write_interest)) {
    FD_CLR(fd, write_interest);
  }

  if (close(fd) < 0) {
    fprintf(stderr, "Failed to close fd %d: close: %s\n", fd, strerror(errno));
    exit(1);
  }
}

int get_peer_pretty_name(struct sockaddr *addr, char *buf, ssize_t buflen) {
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

void sprint_conn(int fd, char *buf, size_t buflen) {
  struct sockaddr_storage cli_addr_store;
  socklen_t cli_addr_size = sizeof(cli_addr_size);

  if (getpeername(fd, (struct sockaddr *)&cli_addr_store, &cli_addr_size) ==
      -1) {
    fprintf(stderr, "Failed to get peer address (fd = %d), errorno: %s\n", fd,
            strerror(errno));
  }

  if (get_peer_pretty_name((struct sockaddr *)(&cli_addr_store), buf, buflen) ==
      -1) {
    fprintf(stderr, "Failed to stringify peer address.\n");
  }
}

void print_accept_conn(int cli_skt) {
  sprint_conn(cli_skt, peer_name_buf, sizeof(peer_name_buf));
  fprintf(stderr, "Accepted new connection from %s\n", peer_name_buf);
}

struct conn_traverse_closure {
  int *max_fd;
};
void conn_travese_accessor(int fd, int idx, void *closure) {
  sprint_conn(fd, peer_name_buf, sizeof(peer_name_buf));
  fprintf(stderr, "[%d] fd=%d address=%s\n", idx, fd, peer_name_buf);
  FD_SET(fd, read_interest);
  struct conn_traverse_closure *ctx = closure;
  if (fd > *(ctx->max_fd)) {
    fprintf(stderr, "max_fd update: %d -> %d\n", *(ctx->max_fd), fd);
    *(ctx->max_fd) = fd;
  }
}

struct conn_activity_check_closure {
  int num_actives;
  conn_manage_ctx cm_ctx;
};
void conn_check_ready_accessor(int fd, int idx, void *closure) {
  struct conn_activity_check_closure *ctx = closure;
  sprint_conn(fd, peer_name_buf, sizeof(peer_name_buf));
  fprintf(stderr, "Checking activity from fd=%d address=%s...  ", fd,
          peer_name_buf);
  if (FD_ISSET(fd, read_interest)) {
    ++ctx->num_actives;
    fprintf(stderr, "Positive.\n");

    int nbytes = read(fd, read_buf, sizeof(read_buf));
    if (nbytes > 0) {
      fprintf(stderr, "Got %d bytes from fd=%d address=%s, emitting now.\n",
              nbytes, fd, peer_name_buf);
      int nbytes_written = write(STDOUT_FILENO, read_buf, nbytes);
      if (nbytes_written < 0) {
        fprintf(stderr, "Unknown error: write: %s\n", strerror(errno));
      } else if (nbytes_written > 0) {
        fprintf(stderr, "Wrote %d bytes to stdout.\n", nbytes_written);
      } else {
        fprintf(stderr, "Got EOF from stdout, exitting...\n");
        exit(0);
      }
    } else if (nbytes < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        fprintf(stderr, "Unknown error: read: %s\n", strerror(errno));
        exit(1);
      }
    } else {
      fprintf(stderr, "Got EOF from fd=%d address=%s, would close it.\n", fd,
              peer_name_buf);

      cm_ctx_conn_mark_dead(ctx->cm_ctx, fd);
    }
  } else {
    fprintf(stderr, "Negative.\n");
  }
}

void init_interests() {
  FD_ZERO(read_interest);
  fprintf(stderr, "read_interest at 0x%016lx sized %ld is intialized.\n",
          (unsigned long)read_interest, sizeof(read_fdset_storage));
  FD_ZERO(write_interest);
  fprintf(stderr, "write_interest at 0x%016lx sized %ld is initialized.\n",
          (unsigned long)write_interest, sizeof(write_fdset_storage));
}

void set_io_non_block(int fd) {
  int io_flags;
  io_flags = fcntl(fd, F_GETFL);
  if (io_flags == -1) {
    fprintf(stderr, "Can't read io flags of fd %d.\n", fd);
    exit(1);
  }

  io_flags = io_flags | O_NONBLOCK;
  if (fcntl(STDIN_FILENO, F_SETFL, io_flags) == -1) {
    fprintf(stderr, "Failed to set io flags of fd %d.\n", fd);
    exit(1);
  }

  fprintf(stderr, "fd %d is now O_NONBLOCK\n", fd);
}

int main(int argc, char *argv[]) {
  init_interests();

  conn_manage_ctx cm_ctx = cm_ctx_create();
  if (cm_ctx == NULL) {
    fprintf(stderr, "Failed to create conn manager.\n");
    exit(1);
  }

  int status;
  if (argc <= 1) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(1);
  }

  char *port = argv[1];
  fprintf(stderr, "Port number is: %s\n", port);

  int srv_skt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  set_io_non_block(srv_skt);

  if (srv_skt == -1) {
    fprintf(stderr, "Failed to open socket. %s\n", strerror(errno));
    exit(1);
  }

  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  status = getaddrinfo(NULL, port, &hints, &res);
  if (status == -1) {
    fprintf(stderr, "Failed to getaddrinfo. %s\n", strerror(errno));
    exit(1);
  }

  status = bind(srv_skt, res->ai_addr, res->ai_addrlen);
  if (status == -1) {
    fprintf(stderr, "Failed to bind. %s\n", strerror(errno));
    exit(1);
  }

  int backlog = 20;
  status = listen(srv_skt, backlog);
  if (status == -1) {
    fprintf(stderr, "Failed to listen. %s\n", strerror(errno));
    exit(1);
  }

  struct timeval *timeout = NULL;
  while (1) {
    int max_fd = srv_skt;
    FD_SET(srv_skt, read_interest);

    if (cm_ctx_get_num_conns(cm_ctx) > 0) {
      struct conn_traverse_closure closure;
      closure.max_fd = &max_fd;
      fprintf(stderr, "Traversing connections:\n");
      cm_ctx_traverse(cm_ctx, &closure, conn_travese_accessor);
    }

    fprintf(stderr, "Waiting for IO activity.\n");
    int nfds = max_fd + 1;
    if (select(nfds, read_interest, write_interest, NULL, timeout) == -1) {
      fprintf(stderr, "Error returned from select: %s\n", strerror(errno));
      exit(1);
    }

    if (FD_ISSET(srv_skt, read_interest)) {
      fprintf(stderr, "Server socket is now readable.\n");
      struct sockaddr_storage cli_addr_store;
      socklen_t cli_addr_size = sizeof(cli_addr_size);

      int cli_skt =
          accept(srv_skt, (struct sockaddr *)(&cli_addr_store), &cli_addr_size);
      if (cli_skt == -1) {
        fprintf(stderr, "Error occurred while accepting client connection.\n");
        continue;
      }

      print_accept_conn(cli_skt);

      set_io_non_block(cli_skt);

      cm_ctx_add_conn(cm_ctx, cli_skt);
      fprintf(stderr, "Now we have %d connections.\n",
              cm_ctx_get_num_conns(cm_ctx));
    }

    if (cm_ctx_get_num_conns(cm_ctx) > 0) {
      fprintf(stderr, "Checking IO activity of client connections:\n");
      struct conn_activity_check_closure closure;
      closure.num_actives = 0;
      closure.cm_ctx = cm_ctx;
      cm_ctx_traverse(cm_ctx, &closure, conn_check_ready_accessor);
      if (closure.num_actives == 0) {
        fprintf(stderr, "No activity.\n");
      }
    }

    fprintf(stderr, "GC: Cleaning dead connections...");
    cm_ctx_gc(cm_ctx, close_fd_or_panic);
  }

  close(srv_skt);

  return 0;
}