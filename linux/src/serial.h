#ifndef SERIAL_H
#define SERIAL_H

#include <stddef.h>

typedef enum {
  SERIAL_DATA = 0,       /* *n_out bytes were read */
  SERIAL_TIMEOUT,        /* nothing within timeout_ms, or interrupted by a signal */
  SERIAL_DISCONNECTED,   /* device gone (unplug, USB reset): close and reopen */
  SERIAL_ERROR,          /* other failure; errno is set */
} serial_result_t;

/* Opens path as a raw 115200 8N1 port with exclusive access and discards any
   input buffered before the open. Returns an fd, or -1 with errno set. */
int serial_open(const char *path);

/* Waits up to timeout_ms (-1 = forever) for input and reads at most len bytes.
   *n_out is set only for SERIAL_DATA. Bytes are passed through untranslated,
   so lines arrive with the firmware's "\r\n". */
serial_result_t serial_read(int fd, void *buf, size_t len, int timeout_ms,
                            size_t *n_out);

void serial_close(int fd);

#endif
