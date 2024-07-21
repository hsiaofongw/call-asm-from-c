#include <error.h>
#include <event2/event.h>
#include <memory.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

#define MAX_READ_BUF 1024
char io_stage_buf[MAX_READ_BUF];

struct io_cb_closure {
  char *buf;
  int start_offset;
  int size;
  int capacity;
};

void on_stdin_activity(int fd, short flags, void *closure) {
  fprintf(stderr, "stdin is now ready to read.\n");
  char buf[MAX_READ_BUF];
  while (1) {
    int result = read(fd, buf, sizeof(buf));
    if (result == 0) {
      fprintf(stderr, "Bye.\n");
      exit(0);
    } else if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        fprintf(stderr, "read: %s\n", strerror(errno));
        exit(1);
      }
      fprintf(stderr,
              "stdin is drained (for now), we would come here later (when it "
              "goes up again).\n");
      break;
    } else {
      fprintf(stderr, "Got %d bytes from stdin.\n", result);
      struct io_cb_closure *c = closure;
      int exceeded = cp_to_ring_buf(c->buf, &(c->start_offset), &(c->size),
                                    c->capacity, buf, result);
      if (exceeded > 0) {
        fprintf(
            stderr,
            "Warning: ringbuf 0x%016lx is fullfilled, %d bytes of data that is "
            "written in "
            "earlist would be overwritten.\n",
            (unsigned long)(c->buf), exceeded);
      }
    }
  }
}

void on_stdout_ready_to_write(int fd, short flags, void *closure) {
  fprintf(stderr, "stdout is now ready to write.\n");
  struct io_cb_closure *c = closure;
  while (1) {
    char buf[MAX_READ_BUF];
    int chunk_size = get_chunk_from_ring_buf(
        buf, sizeof(buf), c->buf, &(c->start_offset), &(c->size), c->capacity);
    if (chunk_size == 0) {
      break;
    }

    fprintf(stderr, "Got %d bytes chunk from the ring buffer.\n", chunk_size);

    int result = write(fd, buf, chunk_size);
    if (result == 0) {
      fprintf(stderr, "Bye.\n");
      exit(0);
    } else if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        fprintf(stderr, "write: %s\n", strerror(errno));
        exit(1);
      }
      fprintf(stderr,
              "stdout is busy for now, we would come here later (when it "
              "goes up again).\n");
      break;
    } else {
      fprintf(stderr, "Emitted %d bytes to stdout from the ring buffer.\n",
              result);
      if (result < chunk_size) {
        // need to insert them back to the ringbuf
        fprintf(stderr, "Returning %d bytes chunk to the ring buffer.\n",
                chunk_size - result);
        return_chunk_to_ring_buf(c->buf, &(c->start_offset), &(c->size),
                                 c->capacity, buf, chunk_size - result);
      }
    }
  }
}

int main() {
  struct event_base *ev_base = event_base_new();
  if (ev_base == NULL) {
    fprintf(stderr, "Failed to create event base.\n");
    exit(1);
  }

  struct io_cb_closure closure = {.buf = io_stage_buf,
                                  .capacity = sizeof(io_stage_buf),
                                  .size = 0,
                                  .start_offset = 0};

  set_io_non_block(STDIN_FILENO);
  set_io_non_block(STDOUT_FILENO);

  while (1) {
    struct event *stdin_ev =
        event_new(ev_base, STDIN_FILENO, EV_READ, on_stdin_activity, &closure);
    if (event_add(stdin_ev, NULL) != 0) {
      fprintf(stderr, "Failed to register read event to stdin.\n");
      exit(1);
    } else {
      fprintf(stderr, "stdin read interest is registered.\n");
    }

    if (closure.size > 0) {
      fprintf(
          stderr,
          "IO buf is non-empty, getting a chance to emit them to stdout...\n");
      struct event *stdout_ev = event_new(ev_base, STDOUT_FILENO, EV_WRITE,
                                          on_stdout_ready_to_write, &closure);
      if (event_add(stdout_ev, NULL) != 0) {
        fprintf(stderr, "Failed to register write event to stdout.\n");
        exit(1);
      } else {
        fprintf(stderr, "stdout write interest is registered.\n");
      }
    }

    fprintf(stderr, "Waiting IO activity...\n");
    int evb_loop_flags = EVLOOP_ONCE;
    event_base_loop(ev_base, evb_loop_flags);
  }

  event_base_free(ev_base);

  return 0;
}