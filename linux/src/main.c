#include "serial.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
  const char *path = argc > 1 ? argv[1] : "/dev/ttyACM0";

  int fd = serial_open(path);
  if (fd < 0) {
    perror(path);
    return EXIT_FAILURE;
  }
  fprintf(stderr, "opened %s\n", path);

  char buf[256];
  for (;;) {
    size_t n;
    serial_result_t r = serial_read(fd, buf, sizeof buf, 1000, &n);

    switch (r) {
    case SERIAL_DATA:
      printf("[%zu] ", n);
      fwrite(buf, 1, n, stdout);
      fflush(stdout);
      break;
    case SERIAL_TIMEOUT:
      fprintf(stderr, "(timeout)\n");
      break;
    case SERIAL_DISCONNECTED:
      fprintf(stderr, "disconnected\n");
      serial_close(fd);
      return EXIT_FAILURE;
    case SERIAL_ERROR:
      perror("serial_read");
      serial_close(fd);
      return EXIT_FAILURE;
    }
  }
}
