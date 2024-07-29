#include <error.h>
#include <event2/event.h>
#include <libgen.h>
#include <memory.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "llist.h"
#include "ringbuf.h"
#include "util.h"

#define MAX_READ_BUF ((0x1UL) << 10)
#define MAX_WRITE_BUF_PER_CONN (((0x1UL) << 20) * 32)
#define MAX_SERVER_WRITE_BUF (((0x1UL) << 10) * 512)

char io_stage_buf[MAX_READ_BUF];

#define MAX_LISTEN_BACKLOG 20

struct server_ctx;
struct conn_ctx {
  int fd;
  ringbuf *read_buf;
  ringbuf *write_buf;
  struct server_ctx *srv;
  struct event *write_event;
  struct event *read_event;

  // never read from a file that is not readable
  // also never write to a file that is not writable
  // a network socket usually be both readable and writable, whilst stdin,
  // stdout do now, at least you can never get input from stdout!
  int readable;
  int writable;

  // for stdin, after_freed means server shutdown,
  // for ordinary network socket, after_freed means simply close socket.
  void (*after_freed)(int fd);
};

void after_stdin_close(int fd) {
  fprintf(stderr, "Bye!\n");
  exit(0);
}

void after_network_socket_close(int fd) {
  close(fd);
  fprintf(stderr, "Socket fd %d is closed.\n", fd);
}

struct server_ctx {
  llist_t **all_conns;
  struct event_base *evb;
  int server_socket;
  ringbuf *write_buf;
  struct event *write_event;
};

struct conn_ctx *conn_ctx_create(int fd) {
  struct conn_ctx *c = malloc(sizeof(struct conn_ctx));
  c->fd = fd;
  c->write_event =
      NULL;  // this is intended, write_event are register on-demand.
  c->read_buf = ringbuf_create(MAX_READ_BUF);
  c->write_buf = ringbuf_create(MAX_WRITE_BUF_PER_CONN);
  c->after_freed = NULL;

  return c;
}

void conn_ctx_free(struct conn_ctx *c) {
  void *after_free_cb = c->after_freed;
  int fd = c->fd;
  if (c->read_buf != NULL) {
    ringbuf_free(c->read_buf);
    c->read_buf = NULL;
  }
  if (c->write_buf != NULL) {
    ringbuf_free(c->write_buf);
    c->write_buf = NULL;
  }
  free(c);
  if (after_free_cb != NULL) {
    void (*cb)(int fd) = after_free_cb;
    cb(fd);
  }
}

int conn_ctx_list_elem_finder(void *payload, int idx, void *closure) {
  return payload == closure ? 1 : 0;
}

void conn_ctx_list_elem_deleter(void *payload, void *closure) {
  struct conn_ctx *c = payload;
  conn_ctx_free(c);
}

void on_file_eof(struct conn_ctx *c_ctx) {
  if (c_ctx->read_event != NULL) {
    event_del(c_ctx->read_event);
    event_free(c_ctx->read_event);
    c_ctx->read_event = NULL;
  }

  if (c_ctx->write_event != NULL) {
    event_del(c_ctx->write_event);
    event_free(c_ctx->write_event);
    c_ctx->write_event = NULL;
  }

  int delete_all = 0;
  list_elem_find_and_remove(c_ctx->srv->all_conns, c_ctx,
                            conn_ctx_list_elem_finder, NULL,
                            conn_ctx_list_elem_deleter, delete_all);
}

void on_ready_to_read(int fd, short flags, void *closure) {
  struct conn_ctx *c_ctx = closure;

  fprintf(stderr, "fd %d is now ready to read.\n", fd);
  char buf[MAX_READ_BUF];
  while (1) {
    int max_read = sizeof(buf);
    const int remain_cap = ringbuf_get_remaining_capacity(c_ctx->read_buf);

    if (remain_cap <= 0) {
      event_del(c_ctx->read_event);
      break;
    }

    if (remain_cap < max_read) {
      max_read = remain_cap;
    }

    int result = read(fd, buf, max_read);
    if (result == 0) {
      fprintf(stderr, "Got EOF from fd %d\n", fd);
      on_file_eof(c_ctx);
      break;
    } else if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        fprintf(stderr, "read: %s\n", strerror(errno));
        exit(1);
      }
      fprintf(stderr,
              "fd %d is drained (for now), we would come here later (when it "
              "goes up again).\n",
              fd);
      break;
    } else {
      fprintf(stderr, "Got %d bytes from fd %d.\n", result, fd);
      int exceeded = ringbuf_send_chunk(c_ctx->read_buf, buf, result);
      if (exceeded > 0) {
        fprintf(stderr,
                "Warning: ringbuf 0x%016lx is fullfilled, and this is "
                "unexpected, %d bytes of data that is "
                "written in "
                "earlist has been overwritten.\n",
                (unsigned long)(c_ctx->read_buf), exceeded);
      }
    }
  }
}

void on_ready_to_write(int fd, short flags, void *closure) {
  fprintf(stderr, "fd %d is now ready to write.\n", fd);
  struct conn_ctx *c_ctx = closure;
  while (1) {
    if (ringbuf_is_empty(c_ctx->write_buf)) {
      fprintf(
          stderr,
          "write_buf of fd %d is drained, removing its write interest now.\n",
          fd);
      event_del(c_ctx->write_event);
      break;
    }

    char buf[MAX_READ_BUF];
    int chunk_size = ringbuf_receive_chunk(buf, sizeof(buf), c_ctx->write_buf);
    fprintf(stderr, "Got %d bytes chunk from the ring buffer.\n", chunk_size);

    int result = write(fd, buf, chunk_size);
    if (result == 0) {
      fprintf(stderr,
              "Got EOF from fd %d, this means the file (or network socket) "
              "is closed, releasing the corresponding connection context.\n",
              fd);
      on_file_eof(c_ctx);
      break;
    } else if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        fprintf(stderr, "write: %s\n", strerror(errno));
        exit(1);
      }
      fprintf(stderr,
              "fd %d is busy for now, we would come here later (when it "
              "goes up again).\n",
              fd);
      break;  // return to event loop
    } else {
      fprintf(stderr, "Emitted %d bytes to fd %d.\n", result, fd);
      if (result < chunk_size) {
        // need to insert them back to the ringbuf
        fprintf(stderr, "Returning %d bytes chunk to the ring buffer.\n",
                chunk_size - result);
        ringbuf_return_chunk(c_ctx->write_buf, &buf[result],
                             chunk_size - result);
      }
    }
  }
}

void register_read_interest(struct server_ctx *this, int fd,
                            void (*after_freed)(int), int readable,
                            int writable) {
  struct event_base *evb = this->evb;
  set_io_non_block(fd);
  struct conn_ctx *c_ctx = conn_ctx_create(fd);
  c_ctx->srv = this;
  c_ctx->after_freed = after_freed;
  c_ctx->readable = readable;
  c_ctx->writable = writable;

  struct event *ev =
      event_new(evb, fd, EV_READ | EV_PERSIST, on_ready_to_read, c_ctx);
  if (ev == NULL) {
    fprintf(stderr, "Failed to create event object for fd %d\n", fd);
    exit(1);
  }
  c_ctx->read_event = ev;

  if (event_add(ev, NULL) != 0) {
    fprintf(stderr, "Failed to register read event to fd %d\n", fd);
    exit(1);
  }

  *this->all_conns = list_insert_payload(*this->all_conns, c_ctx);
  fprintf(stderr, "Registered read interest for fd %d\n", fd);
}

void register_stdin_read_interest(struct server_ctx *srv) {
  register_read_interest(srv, STDIN_FILENO, after_stdin_close, 1, 0);
}

void register_stdout_write_interest(struct server_ctx *srv) {
  struct event_base *evb = srv->evb;
  set_io_non_block(STDOUT_FILENO);
  struct conn_ctx *c_ctx = conn_ctx_create(STDOUT_FILENO);
  c_ctx->after_freed = NULL;
  c_ctx->readable = 0;
  c_ctx->writable = 1;
  c_ctx->srv = srv;

  *srv->all_conns = list_insert_payload(*srv->all_conns, c_ctx);
}

void on_ready_to_accept(int srv_skt, short libev_flags, void *closure) {
  fprintf(stderr,
          "Server (fd %d) is now ready to accept new incoming connection.\n",
          srv_skt);

  struct sockaddr_storage cli_addr_store;
  socklen_t cli_addr_size = sizeof(cli_addr_size);
  int cli_fd =
      accept(srv_skt, (struct sockaddr *)&cli_addr_store, &cli_addr_size);
  if (cli_fd == -1) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      fprintf(stderr,
              "Unknown error when accepting client connection: accept: %s\n",
              strerror(errno));
      exit(1);
    }

    // simply because server is not ready to accept new incoming connection for
    // now, so we will go back to event loop and come back to here later.
    return;
  }

  char peer_addr[INET6_ADDRSTRLEN * 2];
  sprint_conn(peer_addr, sizeof(peer_addr), cli_fd);
  fprintf(stderr, "Accepted connection from %s, fd %d\n", peer_addr, cli_fd);
  struct server_ctx *srv = closure;
  register_read_interest(srv, cli_fd, after_network_socket_close, 1, 1);
}

void register_accept_conn_interest(struct server_ctx *srv) {
  short ev_flags = 0;
  ev_flags |= EV_READ;
  ev_flags |= EV_PERSIST;

  struct event *ev = event_new(srv->evb, srv->server_socket, ev_flags,
                               on_ready_to_accept, srv);
  if (ev == NULL) {
    fprintf(stderr, "Failed to create server accpet event.\n");
    exit(1);
  }

  if (event_add(ev, NULL) != 0) {
    fprintf(stderr, "Failed to add server accept event.\n");
    exit(1);
  }
}

int server_socket_bootstrap(char *port) {
  int srv_skt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (srv_skt == -1) {
    fprintf(stderr, "Failed to create server socket: socket: %s.\n",
            strerror(errno));
    exit(1);
  }

  set_io_non_block(srv_skt);

  struct addrinfo bind_ai_hints, *bind_ai_res;
  memset(&bind_ai_hints, 0, sizeof(bind_ai_hints));
  bind_ai_hints.ai_family = AF_INET;
  bind_ai_hints.ai_socktype = SOCK_STREAM;
  bind_ai_hints.ai_flags = AI_PASSIVE;
  if (getaddrinfo(NULL, port, &bind_ai_hints, &bind_ai_res) == -1) {
    fprintf(stderr, "getaddrinfo: %s\n", strerror(errno));
    exit(1);
  }

  if (bind(srv_skt, bind_ai_res->ai_addr, bind_ai_res->ai_addrlen) == -1) {
    fprintf(stderr, "bind: %s\n", strerror(errno));
    exit(1);
  }

  if (listen(srv_skt, MAX_LISTEN_BACKLOG) == -1) {
    fprintf(stderr, "listen: %s\n", strerror(errno));
    exit(1);
  }

  return srv_skt;
}

struct server_ctx *server_start(char *port) {
  struct server_ctx *srv = malloc(sizeof(struct server_ctx));

  srv->write_buf = ringbuf_create(MAX_SERVER_WRITE_BUF);

  srv->server_socket = server_socket_bootstrap(port);

  srv->all_conns = (llist_t **)malloc(sizeof(llist_t *));
  *srv->all_conns = list_create();

  srv->evb = event_base_new();
  if (srv->evb == NULL) {
    fprintf(stderr, "Failed to create event base.\n");
    exit(1);
  }
  register_accept_conn_interest(srv);
  register_stdin_read_interest(srv);
  register_stdout_write_interest(srv);

  return srv;
}

void server_shutdown(struct server_ctx *srv) {
  list_free(*(srv->all_conns), conn_ctx_list_elem_deleter, NULL);
  free(srv->all_conns);
  event_base_free(srv->evb);
  ringbuf_free(srv->write_buf);

  free(srv);
}

int collect_input_from_each_readbuf(void *payload, int idx, void *closure) {
  struct server_ctx *srv = closure;
  struct conn_ctx *c = payload;
  if (!c->readable) {
    return 1;
  }

  int remain_cap = ringbuf_get_remaining_capacity(srv->write_buf);
  if (remain_cap <= 0) {
    return 1;
  }

  ringbuf_transfer(srv->write_buf, c->read_buf, remain_cap);
  if (!event_pending(c->read_event, EV_READ, NULL)) {
    if (event_add(c->read_event, NULL) != 0) {
      fprintf(stderr, "Failed to register read event to fd %d\n", c->fd);
      exit(1);
    }
  }

  return 1;
}

int emit_to_each_writable_conn(void *payload, int idx, void *closure) {
  struct conn_ctx *c_ctx = payload;
  if (!c_ctx->writable) {
    return 1;
  }

  int remain_cap = ringbuf_get_remaining_capacity(c_ctx->write_buf);
  if (remain_cap <= 0) {
    return 1;
  }

  struct server_ctx *srv = closure;
  struct event_base *evb = srv->evb;

  ringbuf_copy(c_ctx->write_buf, srv->write_buf, remain_cap);

  if (!c_ctx->write_event || c_ctx->write_event == NULL) {
    c_ctx->write_event = event_new(evb, c_ctx->fd, EV_WRITE | EV_PERSIST,
                                   on_ready_to_write, c_ctx);
    if (c_ctx->write_event == NULL) {
      fprintf(stderr, "Failed to create event object for fd %d\n", c_ctx->fd);
      exit(1);
    }
  }

  if (!event_pending(c_ctx->write_event, EV_WRITE, NULL)) {
    if (event_add(c_ctx->write_event, NULL) != 0) {
      fprintf(stderr, "Failed to register write event to fd %d\n", c_ctx->fd);
      exit(1);
    }
  }

  return 1;
}

int server_run(struct server_ctx *srv) {
  while (1) {
    fprintf(stderr, "Waiting IO activity...\n");
    int evb_loop_flags = EVLOOP_ONCE;
    event_base_loop(srv->evb, evb_loop_flags);

    // 只有确保 server 的 write_buf 能容纳所有 client 的
    // read_buf，才去 collect 每个 read_buf 的内容到 server 的 write_buf。
    // 因为，如果 client 的 read_buf 满了，server 就会调用 event_del
    // 移除 read interest，也就是停止（调用 read 方法）从 client
    // 读入更多数据，迫使 client 减轻向 server
    // 的发包速率和发包流量（发慢点、发少点），某种程度上来说你可以把这理解为一种反压措施。
    const int sum_of_cli_read_buf_size =
        list_get_size(*srv->all_conns) * MAX_READ_BUF;
    if (ringbuf_get_remaining_capacity(srv->write_buf) >=
        sum_of_cli_read_buf_size) {
      list_traverse_payload(*srv->all_conns, srv,
                            collect_input_from_each_readbuf);
    }

    // 检查 server 的 write_buf
    // 是否需要动态扩容（正如刚才所说，它需要具备容纳所有 client 的 read_buf
    // 的能力）
    const int curr_srv_write_buf_size = ringbuf_get_capacity(srv->write_buf);
    if (curr_srv_write_buf_size < sum_of_cli_read_buf_size) {
      int new_size = ringbuf_upscale_if_needed(&(srv->write_buf),
                                               sum_of_cli_read_buf_size);
      if (new_size > curr_srv_write_buf_size) {
        fprintf(stderr, "Server's write_buf has been up-scaled to %d bytes\n",
                new_size);
      }
    }

    if (!ringbuf_is_empty(srv->write_buf)) {
      list_traverse_payload(*srv->all_conns, srv, emit_to_each_writable_conn);
      ringbuf_clear(srv->write_buf);
    }
  }

  server_shutdown(srv);
  return 0;
}

void print_usage(char *argv[]) {
  char *execname = basename(argv[0]);
  fprintf(stderr, "Usage:\n\n  %s -l <port>\n  %s -c <host>:<port>", execname,
          execname);
}

int main(int argc, char *argv[]) {
  if (argc <= 2) {
    print_usage(argv);
    exit(1);
  }
  if (strcmp(argv[1], "-l") == 0) {
    fprintf(stderr, "Launching in server mode.\n");
    char *port = argv[2];
    struct server_ctx *srv = server_start(port);
    fprintf(stderr, "Server listening on %s\n", port);
    return server_run(srv);
  } else if (strcmp(argv[1], "-c") == 0) {
    fprintf(stderr, "Launching in client mode.\n");
    exit(0);
  } else {
    print_usage(argv);
    exit(1);
  }
}