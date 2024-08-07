#include <error.h>
#include <event2/event.h>
#include <libgen.h>
#include <memory.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <unistd.h>

#include "err.h"
#include "llist.h"
#include "pkt.h"
#include "priority_queue.h"
#include "queue.h"
#include "ringbuf.h"
#include "util.h"

#define MAX_READ_BUF ((0x1UL) << 10)
#define MAX_WRITE_BUF_PER_CONN (((0x1UL) << 20) * 32)
#define MAX_SERVER_WRITE_BUF (((0x1UL) << 10) * 512)
#define MAX_RX_PACKETS_QUEUE 16
#define MAX_TX_PACKETS_QUEUE 16
#define MAX_READ_CHUNK_SIZE 128
#define MAX_NAME_LENGTH 32

char io_stage_buf[MAX_READ_BUF];
char client_name[MAX_NAME_LENGTH];

#define MAX_LISTEN_BACKLOG 20

struct server_ctx;
struct conn_ctx {
  int fd;
  ringbuf *read_buf;
  ringbuf *write_buf;
  struct server_ctx *srv;
  struct event *write_event;
  struct event *read_event;

  parse_ctx *p_ctx;
  serialize_ctx *s_ctx;

  queue *rx_packets;

  // number of packets move to server's packet queue
  int nr_received;

  queue *tx_packets;

  int nr_transmitted;
  // number of packets move from server's packet queue

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
  queue *tx_packets;
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
  c->p_ctx = NULL;
  c->rx_packets = queue_create(MAX_RX_PACKETS_QUEUE);
  c->tx_packets = queue_create(MAX_TX_PACKETS_QUEUE);
  c->nr_received = 0;
  c->nr_transmitted = 0;

  struct alloc_t *allocator = get_default_allocator();

  c->s_ctx = serialize_ctx_create(allocator);

  int status = parse_ctx_create(&(c->p_ctx), allocator);
  if (status != 0) {
    fprintf(stderr, "Failed to allocate parse_ctx for fd %d: %s", fd,
            err_code_2_str(status));
    exit(1);
  }

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

  if (c->p_ctx != NULL) {
    parse_ctx_free(&(c->p_ctx));
  }

  if (c->rx_packets) {
    queue_free(&(c->rx_packets));
  }

  if (c->tx_packets) {
    queue_free(&(c->tx_packets));
  }

  if (c->s_ctx) {
    serialize_ctx_free(c->s_ctx);
    c->s_ctx = NULL;
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

void read_chunk_from_fd(ringbuf *dst, int max_read, int *would_block, int *eof,
                        int fd) {
  char buf[MAX_READ_BUF];

  *eof = 0;
  *would_block = 0;
  while (1) {
    int will_read = MAX(
        0,
        MIN(sizeof(buf), MIN(ringbuf_get_remaining_capacity(dst), max_read)));
    if (will_read == 0) {
      break;
    }

    int rbytes = read(fd, buf, will_read);
    if (rbytes == 0) {
      *eof = 1;
      return;
    }

    if (rbytes < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        // Un-recoverable error.
        fprintf(stderr, "Failed to call read() on fd %d: %s\n", fd,
                strerror(errno));
        exit(1);
      }

      *would_block = 1;
      break;
    }

    max_read -= rbytes;

    // 0 < rbytes < will_read <= remaining_capacity(dst)，
    // 所以这个 chunk 一定能整个放进 ringbuf *dst。
    ringbuf_send_chunk(dst, buf, rbytes);
  }
}

void on_ready_to_read(int fd, short flags, void *closure) {
  struct conn_ctx *c_ctx = closure;

  fprintf(stderr, "fd %d is now ready to read.\n", fd);
  char buf[MAX_READ_BUF];
  int would_block = 0, eof = 0;
  while (1) {
    if (queue_is_fullfilled(c_ctx->rx_packets)) {
      event_del(c_ctx->read_event);
      break;
    }

    read_chunk_from_fd(c_ctx->read_buf, MAX_READ_CHUNK_SIZE, &would_block, &eof,
                       fd);
    if (eof) {
      fprintf(stderr, "Got EOF from fd %d\n", fd);
      on_file_eof(c_ctx);
      return;
    }

    if (would_block) {
      break;
    }

    if (parse_ctx_is_ready_to_send_chunk(c_ctx->p_ctx)) {
      int chunk_size = ringbuf_receive_chunk(buf, sizeof(buf), c_ctx->read_buf);

      int accepted, need_more, status;
      status = parse_ctx_send_chunk(c_ctx->p_ctx, buf, chunk_size, &accepted,
                                    &need_more);
      if (status == ErrNeedMore || status == 0) {
        if (accepted < chunk_size) {
          ringbuf_return_chunk(c_ctx->read_buf, &buf[accepted],
                               chunk_size - accepted);
        }
      }
    }

    if (parse_ctx_is_ready_to_extract_packet(c_ctx->p_ctx)) {
      pkt *p;
      int status = parse_ctx_receive_pkt(c_ctx->p_ctx, &p);
      if (status != 0) {
        fprintf(stderr, "Failed to extract packet: %s\n",
                err_code_2_str(status));
        exit(1);
      }
      queue_enqueue(c_ctx->rx_packets, p);
    }
  }
}

void write_chunk_to_fd(int fd, int max_write, int *would_block, int *eof,
                       ringbuf *src) {
  char buf[MAX_READ_BUF];
  *eof = 0;
  *would_block = 0;
  while (1) {
    int will_write =
        MAX(0, MIN(sizeof(buf), MIN(max_write, ringbuf_get_size(src))));
    if (will_write == 0) {
      // Currently there's no chunk to emit
      break;
    }

    int chunk_size = ringbuf_receive_chunk(buf, will_write, src);
    if (chunk_size == 0) {
      fprintf(stderr, "Unexpected error, chunk_size shouldn't be zero.\n");
      exit(1);
    }
    will_write = MIN(chunk_size, will_write);
    if (will_write == 0) {
      // Currently there's no chunk to emit
      break;
    }

    int wbytes = write(fd, buf, will_write);
    if (wbytes == 0) {
      *eof = 1;
      return;
    }

    if (wbytes < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        // Unrecoverable error.
        fprintf(stderr, "Failed to call write() on fd %d: %s\n", fd,
                strerror(errno));
        exit(1);
      }

      *would_block = 1;
      break;
    }

    max_write -= wbytes;
  }
}

void on_ready_to_write(int fd, short flags, void *closure) {
  fprintf(stderr, "fd %d is now ready to write.\n", fd);
  struct conn_ctx *c_ctx = closure;

  while (1) {
    if (queue_get_size(c_ctx->tx_packets) == 0) {
      event_del(c_ctx->write_event);
      break;
    }

    if (serialize_ctx_is_ready_to_send_pkt(c_ctx->s_ctx)) {
      pkt *p = queue_dequeue(c_ctx->tx_packets);
      serialize_ctx_send_pkt(c_ctx->s_ctx, p);
      pkt_free(&p);
    }

    while (serialize_ctx_is_ready_to_receive_chunk(c_ctx->s_ctx) != 0) {
      char buf[MAX_READ_CHUNK_SIZE];
      int chunk_size;
      int remain_write_buf_cap =
          ringbuf_get_remaining_capacity(c_ctx->write_buf);
      int max_write = MAX(0, MIN(sizeof(buf), remain_write_buf_cap));
      serialize_ctx_receive_chunk(buf, max_write, &chunk_size, c_ctx->s_ctx);
      if (chunk_size == 0) {
        break;
      }

      ringbuf_send_chunk(c_ctx->write_buf, buf, chunk_size);
    }

    if (!ringbuf_is_empty(c_ctx->write_buf)) {
      int would_block, eof;
      write_chunk_to_fd(fd, ringbuf_get_size(c_ctx->write_buf), &would_block,
                        &eof, c_ctx->write_buf);
      if (eof) {
        on_file_eof(c_ctx);
        break;
      }

      if (would_block) {
        break;
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

  srv->tx_packets = queue_create(MAX_TX_PACKETS_QUEUE);
  if (srv->tx_packets == NULL) {
    fprintf(stderr, "Failed to create tx queue on server object.\n");
    exit(1);
  }

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

  return srv;
}

void server_shutdown(struct server_ctx *srv) {
  list_free(*(srv->all_conns), conn_ctx_list_elem_deleter, NULL);
  free(srv->all_conns);
  event_base_free(srv->evb);
  if (srv->tx_packets) {
    queue_free(&(srv->tx_packets));
  }

  free(srv);
}

int collect_packets_from_each_rx_queue(void *payload, int idx, void *closure) {
  struct server_ctx *srv = closure;
  struct conn_ctx *c = payload;
  if (!c->readable) {
    return 1;
  }

  int remain_cap = ringbuf_get_remaining_capacity(NULL);
  if (remain_cap <= 0) {
    return 1;
  }

  ringbuf_transfer(NULL, c->read_buf, remain_cap);
  if (!event_pending(c->read_event, EV_READ, NULL)) {
    if (event_add(c->read_event, NULL) != 0) {
      fprintf(stderr, "Failed to register read event to fd %d\n", c->fd);
      exit(1);
    }
  }

  return 1;
}

int compare_conn_rx_nr(void *a, void *b, void *closure) {
  struct conn_ctx *ca = a;
  struct conn_ctx *cb = b;
  return ca->nr_received <= cb->nr_received;
}

int compare_conn_tx_nr(void *a, void *b, void *closure) {
  struct conn_ctx *ca = a;
  struct conn_ctx *cb = b;
  return ca->nr_transmitted <= cb->nr_transmitted;
}

int append_conn_to_ingest_queue(void *payload, int idx, void *closure) {
  struct conn_ctx *c = payload;
  if (!c->readable) {
    return 1;
  }

  pq *ingest = closure;
  pq_insert(ingest, c);
  return 1;
}

int append_conn_to_egress_queue(void *payload, int idx, void *closure) {
  struct conn_ctx *c = payload;
  if (!c->writable) {
    return 1;
  }

  pq *egress = closure;
  pq_insert(egress, c);
  return 1;
}

int emit_to_each_writable_conn(void *payload, int idx, void *closure) {
  struct conn_ctx *c_ctx = payload;
  if (!c_ctx->writable) {
    return 1;
  }

  if (queue_get_size(c_ctx->tx_packets) == 0) {
    return 1;
  }

  struct server_ctx *srv = closure;
  struct event_base *evb = srv->evb;

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

void collect_rx_queue(struct server_ctx *srv) {
  // 从每个连接的 RX 队列收集到的封包放到 server 的 TX 队列。
  // 在 server 的 TX 队列剩余容量有限的情况下，
  // 应当优先照顾哪些连接的 RX 队列？
  pq *ingest =
      pq_create(list_get_size(*srv->all_conns), compare_conn_rx_nr, NULL);
  if (!ingest) {
    fprintf(stderr, "Failed to allocate queue.\n");
    exit(1);
  }

  // 把每个 conn 按照 nr_received 的顺序放到 priority queue，nr_received
  // 较小的排在前面。
  list_traverse_payload(*srv->all_conns, ingest, append_conn_to_ingest_queue);

  // 从 priority queue 取出之前放进去的每个 conn，然后从它的 rx_packets 把
  // packet 移动到 server 的 packet 队列
  while (!pq_is_empty(ingest)) {
    struct conn_ctx *conn = pq_shift(ingest);
    if (!conn) {
      fprintf(stderr, "Unknown error: conn object is NULL.\n");
      exit(1);
    }

    conn->nr_received += queue_transfer(srv->tx_packets, conn->rx_packets);
  }

  pq_free(&ingest);
  if (ingest) {
    fprintf(stderr, "Unknown error: Failed to free pq object.\n");
    exit(1);
  }
}

void distribute_tx_queue(struct server_ctx *srv) {
  pq *egress =
      pq_create(list_get_size(*srv->all_conns), compare_conn_tx_nr, NULL);
  if (!egress) {
    fprintf(stderr, "Failed to allocate queue.\n");
    exit(1);
  }

  list_traverse_payload(*srv->all_conns, egress, append_conn_to_egress_queue);
  while (!pq_is_empty(egress)) {
    struct conn_ctx *conn = pq_shift(egress);
    if (!conn) {
      fprintf(stderr, "Unknown error: conn object is NULL.\n");
      exit(1);
    }

    conn->nr_transmitted += queue_transfer(conn->tx_packets, srv->tx_packets);
  }

  pq_free(&egress);
  if (egress) {
    fprintf(stderr, "Unknown error: Failed to free pq object.\n");
    exit(1);
  }
}

int server_run(struct server_ctx *srv) {
  while (1) {
    fprintf(stderr, "Waiting IO activity...\n");
    int evb_loop_flags = EVLOOP_ONCE;
    event_base_loop(srv->evb, evb_loop_flags);

    collect_rx_queue(srv);

    if (queue_get_size(srv->tx_packets) > 0) {
      distribute_tx_queue(srv);
    }

    list_traverse_payload(*srv->all_conns, srv, emit_to_each_writable_conn);
  }

  server_shutdown(srv);
  return 0;
}

void print_usage(char *argv[]) {
  char *execname = basename(argv[0]);
  fprintf(stderr,
          "Usage:\n\n"
          "  %s -l <port>\n"
          "  %s -c <host>:<port> <username>",
          execname, execname);
}

int main(int argc, char *argv[]) {
  if (argc <= 2) {
    print_usage(argv);
    exit(1);
  }
  if (argc >= 3 && strcmp(argv[1], "-l") == 0) {
    fprintf(stderr, "Launching in server mode.\n");
    char *port = argv[2];
    struct server_ctx *srv = server_start(port);
    fprintf(stderr, "Server listening on %s\n", port);
    return server_run(srv);
  } else if (argc >= 4 && strcmp(argv[1], "-c") == 0) {
    fprintf(stderr, "Launching in client mode.\n");
    int name_len = strlen(argv[3]);
    if (name_len > sizeof(client_name)) {
      fprintf(stderr, "Username too long!\n");
      exit(1);
    }

    // Make sure the client_name be always null-terminated.
    memset(client_name, 0, sizeof(client_name));
    memcpy(client_name, argv[3], MIN(name_len, sizeof(client_name) - 1));
    fprintf(stderr, "Logged in as \"%s\".\n", client_name);

    return 0;
  } else {
    print_usage(argv);
    exit(1);
  }
}