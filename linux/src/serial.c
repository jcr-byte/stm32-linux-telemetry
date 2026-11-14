#define _DEFAULT_SOURCE   /* cfmakeraw, CRTSCTS */
#include "serial.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

int serial_open(const char *path)
{
  /* O_NONBLOCK so open() can't hang waiting for carrier detect before CLOCAL is set. */
  int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0)
    return -1;

  /* A second opener such as minicom would otherwise silently split the byte stream. */
  if (ioctl(fd, TIOCEXCL) < 0)
    goto fail;

  struct termios tio;
  if (tcgetattr(fd, &tio) < 0)
    goto fail;

  cfmakeraw(&tio);   /* 8 data bits, no parity, no echo, no line editing or CR/LF translation */
  tio.c_cflag &= ~(CSTOPB | CRTSCTS);
  tio.c_cflag |= CLOCAL | CREAD;
  tio.c_cc[VMIN] = 0;   /* never block in read(); serial_read() waits in poll() */
  tio.c_cc[VTIME] = 0;

  if (cfsetispeed(&tio, B115200) < 0 || cfsetospeed(&tio, B115200) < 0)
    goto fail;
  if (tcsetattr(fd, TCSANOW, &tio) < 0)
    goto fail;

  /* Discard input queued before the open, usually the tail of a line. */
  tcflush(fd, TCIFLUSH);
  return fd;

fail:;
  int saved = errno;
  close(fd);
  errno = saved;
  return -1;
}
