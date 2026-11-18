#define _DEFAULT_SOURCE   /* cfmakeraw, CRTSCTS */
#include "serial.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
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

serial_result_t serial_read(int fd, void *buf, size_t len, int timeout_ms,
                            size_t *n_out)
{
  struct pollfd pfd = { .fd = fd, .events = POLLIN };

  int rc = poll(&pfd, 1, timeout_ms);
  if (rc < 0)
    return errno == EINTR ? SERIAL_TIMEOUT : SERIAL_ERROR;
  if (rc == 0)
    return SERIAL_TIMEOUT;

  if (pfd.revents & POLLNVAL) {
    errno = EBADF;
    return SERIAL_ERROR;
  }

  /* Read even on POLLHUP/POLLERR: bytes may still be buffered, and read()'s
     result tells us whether the device is really gone. */
  ssize_t n = read(fd, buf, len);
  if (n > 0) {
    *n_out = (size_t)n;
    return SERIAL_DATA;
  }
  if (n == 0)   /* poll() said ready but nothing came: the tty has hung up */
    return SERIAL_DISCONNECTED;

  switch (errno) {
  case EAGAIN:
  case EINTR:
    return SERIAL_TIMEOUT;
  case EIO:
  case ENXIO:
  case ENODEV:
    return SERIAL_DISCONNECTED;
  default:
    return SERIAL_ERROR;
  }
}
