#include "linebuf.h"

void linebuf_init(linebuf_t *lb)
{
  lb->len = 0;
  lb->discarding = false;
}

linebuf_result_t linebuf_feed(linebuf_t *lb, char c, const char **line)
{
  if (c == '\n') {
    if (lb->discarding) {
    lb->len = 0;
    lb->discarding = false;
    return LINEBUF_PENDING;
    }

    if (lb->len > 0 && lb->buf[lb->len - 1] == '\r') {
      lb->len--;
    }
    lb->buf[lb->len] = '\0';
    *line = lb->buf;
    lb->len = 0;
    return LINEBUF_LINE;
  }

  if (lb->discarding) {
    return LINEBUF_PENDING;
  }

  if (lb->len == LINEBUF_MAX) {
    lb->discarding = true;
    lb->len = 0;
    return LINEBUF_OVERFLOW;
  }

  lb->buf[lb->len] = c;
  lb->len++;
  return LINEBUF_PENDING;
}
