#include "conn_manage.h"

#include <errno.h>
#include <stdlib.h>

#include "llist.h"

struct cm_ctx_impl {
  struct llist_t *fds;
};

struct conn_ctx_impl {
  int fd;
  int dead;
};

conn_manage_ctx cm_ctx_create() {
  struct cm_ctx_impl *cm_ctx = malloc(sizeof(struct cm_ctx_impl));
  cm_ctx->fds = list_create();
  return cm_ctx;
}

struct conn_ctx_impl *conn_ctx_create(int fd) {
  struct conn_ctx_impl *conn = malloc(sizeof(struct conn_ctx_impl));
  conn->fd = fd;
  conn->dead = 0;
  return conn;
}

void conn_ctx_free(struct conn_ctx_impl *conn) { free(conn); }

struct cm_ctx_free_closure {
  void (*before_conn_remove)(int fd);
};

void cm_ctx_free_hook(void *payload, void *closure) {
  struct conn_ctx_impl *conn = payload;
  struct cm_ctx_free_closure *c = closure;
  c->before_conn_remove(conn->fd);
  conn_ctx_free(conn);
}

void cm_ctx_free(conn_manage_ctx cm_ctx, void (*before_conn_remove)(int fd)) {
  struct cm_ctx_free_closure closure;
  closure.before_conn_remove = before_conn_remove;
  struct cm_ctx_impl *impl = cm_ctx;
  list_free(impl->fds, cm_ctx_free_hook, (void *)&closure);
}

void cm_ctx_add_conn(conn_manage_ctx cm_ctx, int fd) {
  struct cm_ctx_impl *impl = (void *)cm_ctx;
  struct conn_ctx_impl *conn = conn_ctx_create(fd);
  impl->fds = list_insert_payload(impl->fds, conn);
}

struct cm_ctx_conn_traverse_closure {
  void (*cb)(int fd, int idx, void *closure);
  void *closure;
};

int cm_ctx_conn_traverse_accessor(struct llist_t *elem, int idx,
                                  void *closure) {
  struct cm_ctx_conn_traverse_closure *wrapped_closure = closure;
  struct conn_ctx_impl *conn = elem->payload;

  if (!conn->dead) {
    wrapped_closure->cb(conn->fd, idx, wrapped_closure->closure);
  }

  return 1;
}

void cm_ctx_traverse(conn_manage_ctx cm_ctx, void *closure,
                     void (*cb)(int fd, int idx, void *closure)) {
  struct cm_ctx_impl *impl = (void *)cm_ctx;
  struct cm_ctx_conn_traverse_closure closure_wrap;
  closure_wrap.cb = cb;
  closure_wrap.closure = closure;
  list_traverse(impl->fds, (void *)&closure_wrap,
                cm_ctx_conn_traverse_accessor);
}

int find_fd_accessor(struct llist_t *elem, int idx, void *closure) {
  int *fd_ptr = closure;
  struct conn_ctx_impl *conn = elem->payload;
  return conn->fd == *fd_ptr ? 1 : 0;
}

int cm_ctx_get_num_conns(conn_manage_ctx cm_ctx) {
  return list_get_size(((struct cm_ctx_impl *)cm_ctx)->fds);
}

int get_max_fd_traverse_accessor(struct llist_t *elem, int idx, void *closure) {
  int *max_fd = closure;
  struct conn_ctx_impl *conn = elem->payload;
  if (conn->fd > *max_fd) {
    *max_fd = conn->fd;
  }
  return 1;
}

int cm_ctx_get_max_fd(conn_manage_ctx cm_ctx) {
  int max_fd = 0;
  struct cm_ctx_impl *impl = (void *)cm_ctx;
  list_traverse(impl->fds, &max_fd, get_max_fd_traverse_accessor);
  return max_fd;
}

int mark_dead_traverse_accessor(struct llist_t *elem, int idx, void *closure) {
  int *fd = (int *)closure;
  struct conn_ctx_impl *conn = elem->payload;
  if (conn->fd == *fd) {
    conn->dead = 1;
    return 0;
  }
  return 1;
}

void cm_ctx_conn_mark_dead(conn_manage_ctx cm_ctx, int fd) {
  struct cm_ctx_impl *impl = cm_ctx;
  list_traverse(impl->fds, &fd, mark_dead_traverse_accessor);
}

int gc_predicate(void *payload, int idx, void *closure) {
  return ((struct conn_ctx_impl *)payload)->dead;
}

struct cm_ctx_gc_closure {
  void (*before_conn_remove)(int fd);
};

void gc_before_elem_delete(void *payload, void *closure) {
  struct conn_ctx_impl *conn = payload;
  struct cm_ctx_gc_closure *c = closure;
  c->before_conn_remove(conn->fd);
}

void cm_ctx_gc(conn_manage_ctx cm_ctx, void (*before_conn_remove)(int fd)) {
  struct cm_ctx_impl *impl = cm_ctx;
  struct cm_ctx_gc_closure closure;
  closure.before_conn_remove = before_conn_remove;
  list_elem_find_and_remove(&(impl->fds), NULL, gc_predicate, &closure,
                            gc_before_elem_delete, 1);
}