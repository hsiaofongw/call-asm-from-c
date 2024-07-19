#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

const int fd_capacity_per_int = sizeof(int) * 8;
int set_storage[3 * FD_SETSIZE / fd_capacity_per_int];

const int max_io_buf_size = 256;
char read_buf[max_io_buf_size];
int write_buf_start = 0;
int write_buf_current_size = 0;
char write_buf[max_io_buf_size];

int get_write_buf_remain_capacity() {
  return sizeof(write_buf) - write_buf_current_size;
}

long get_diff(void *a, void *b) { return (char *)a - (char *)b; }

void print_write_buf_status() {
  fprintf(stderr, "write_buf: start = %d, size = %d, remain cap = %d\n",
          write_buf_start, write_buf_current_size,
          get_write_buf_remain_capacity());
}

int main() {
  fd_set *read_interest = (void *)(&set_storage[0]);
  fd_set *write_interest =
      (void *)(&set_storage[FD_SETSIZE / fd_capacity_per_int]);
  fd_set *err_interest =
      (void *)(&set_storage[2 * FD_SETSIZE / fd_capacity_per_int]);

  FD_ZERO(read_interest);
  fprintf(stderr, "read_interest at 0x%x is intialized\n", read_interest);
  FD_ZERO(write_interest);
  fprintf(stderr, "write_interest at 0x%x is initialized, diff = 0x%x\n",
          write_interest, get_diff(write_interest, read_interest));
  FD_ZERO(err_interest);
  fprintf(stderr, "err_interest at 0x%x is initialized, diff = 0x%x\n",
          err_interest, get_diff(err_interest, write_interest));

  int max_fd = STDOUT_FILENO;
  int nfds = max_fd + 1;
  struct timeval *timeout = NULL;

  int io_flags;
  io_flags = fcntl(STDIN_FILENO, F_GETFL);
  if (io_flags == -1) {
    fprintf(stderr, "Can't read io flags of stdin.\n");
    exit(1);
  }

  io_flags = io_flags | O_NONBLOCK;
  if (fcntl(STDIN_FILENO, F_SETFL, io_flags) == -1) {
    fprintf(stderr, "Failed to set io flags of stdin.\n");
    exit(1);
  }

  fprintf(stderr, "stdin is now O_NONBLOCK\n");

  io_flags = fcntl(STDOUT_FILENO, F_GETFL);
  if (io_flags == -1) {
    fprintf(stderr, "Can't get io flags of stdout.\n");
    exit(1);
  }

  if (fcntl(STDOUT_FILENO, F_SETFL, io_flags) == -1) {
    fprintf(stderr, "Failed to set io flags of stdout.\n");
    exit(1);
  }

  fprintf(stderr, "stdout is now O_NONBLOCK\n");

  while (1) {
    FD_SET(STDIN_FILENO, read_interest);
    FD_SET(STDOUT_FILENO, write_interest);

    FD_SET(STDIN_FILENO, err_interest);
    FD_SET(STDOUT_FILENO, err_interest);
    select(nfds, read_interest, write_interest, err_interest, timeout);

    if (FD_ISSET(STDIN_FILENO, err_interest)) {
      fprintf(stderr, "Exception on stdin, exitting...\n");
      exit(1);
    }

    if (FD_ISSET(STDOUT_FILENO, err_interest)) {
      fprintf(stderr, "Exception on stdout, exitting...\n");
      exit(1);
    }

    if (FD_ISSET(STDOUT_FILENO, write_interest) && write_buf_current_size > 0) {
      fprintf(
          stderr,
          "stdout is now ready to write and we have non-empty write buffer.\n");

      int write_result = 0;
      while (write_buf_current_size > 0) {
        int max_write = write_buf_current_size;
        if (write_buf_start + max_write > sizeof(write_buf)) {
          max_write = sizeof(write_buf) - write_buf_start;
        }
        write_result =
            write(STDOUT_FILENO, &write_buf[write_buf_start], max_write);
        if (write_result == 0) {
          fprintf(stderr, "Got EOF from stdout, exitting...\n");
          return 0;
        } else if (write_result < 0) {
          if (errno != EAGAIN && errno != EWOULDBLOCK) {
            fprintf(stderr,
                    "Un-expected error on stdin, errorno = %d, exitting...\n",
                    errno);
            exit(1);
          }
          fprintf(stderr, "stdout is blocking now, would try again later.\n");
          break;
        } else {
          fprintf(stderr, "Wrote %d bytes to stdout.\n", write_result);
          write_buf_start =
              (write_buf_start + write_result) % sizeof(write_buf);
          write_buf_current_size -= write_result;
          print_write_buf_status();
        }
      }
    }

    if (FD_ISSET(STDIN_FILENO, read_interest)) {
      fprintf(stderr, "stdin is now ready to read.\n");

      int bytes_read = read(STDIN_FILENO, read_buf, max_io_buf_size);
      if (bytes_read == 0) {
        fprintf(stderr, "Got EOF from stdin, exitting...\n");
        return 0;
      } else if (bytes_read < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
          fprintf(stderr,
                  "Un-expected error on stdin, errorno = %d, exitting...\n",
                  errno);
          exit(1);
        }
        fprintf(stderr, "stdin is blocking now, would try again later.\n");
      } else {
        fprintf(stderr,
                "Got %d bytes from stdin, writing them to write_buf...\n",
                bytes_read);
        int nr_exceed_bytes = bytes_read - get_write_buf_remain_capacity();
        if (nr_exceed_bytes > 0) {
          fprintf(stderr,
                  "%d bytes exceeded to current write buf, the data that is "
                  "wrote earlier "
                  "would get dropped.\n",
                  nr_exceed_bytes);
        }

        for (int i = 0,
                 dst_offset = (write_buf_start + write_buf_current_size) %
                              sizeof(write_buf);
             i < bytes_read; ++i) {
          write_buf[(dst_offset + i) % sizeof(write_buf)] = read_buf[i];
        }

        write_buf_current_size += bytes_read;
        if (write_buf_current_size > sizeof(write_buf)) {
          write_buf_start =
              (write_buf_start + write_buf_current_size - sizeof(write_buf)) %
              sizeof(write_buf);
          write_buf_current_size = sizeof(write_buf);
        }
        print_write_buf_status();
      }
    }
  }

  return 0;
}
