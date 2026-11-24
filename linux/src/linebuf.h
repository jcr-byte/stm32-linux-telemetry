#ifndef LINEBUF_H
#define LINEBUF_H

#include <stddef.h>
#include <stdbool.h>

#define LINEBUF_MAX 128

typedef struct {
  char buf[LINEBUF_MAX + 1];
  size_t len;
  bool discarding;
} linebuf_t;

typedef enum {
  LINEBUF_PENDING,
  LINEBUF_LINE,
  LINEBUF_OVERFLOW,
} linebuf_result_t;

void linebuf_init(linebuf_t *lb);
linebuf_result_t linebuf_feed(linebuf_t *lb, char c, const char **line);

#endif
