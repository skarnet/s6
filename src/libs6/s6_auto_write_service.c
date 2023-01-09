/* ISC license. */

#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <skalibs/types.h>
#include <skalibs/buffer.h>
#include <skalibs/djbunix.h>
#include <skalibs/strerr.h>

#include <s6/auto.h>

void s6_auto_write_service (char const *dir, unsigned int nfd, s6_buffer_writer_func_ref f, void *data, char const *logger)
{
  int fd ;
  buffer b ;
  size_t dirlen = strlen(dir) ;
  mode_t m = umask(0) ;
  char buf[4096] ;
  char fn[dirlen + 17] ;
  if (mkdir(dir, 0755) == -1) strerr_diefu2sys(111, "mkdir ", dir) ;
  umask(m) ;
  memcpy(fn, dir, dirlen) ;
  if (nfd)
  {
    char fmt[UINT_FMT] ;
    size_t l = uint_fmt(fmt, nfd) ;
    fmt[l++] = '\n' ;
    memcpy(fn + dirlen, "/notification-fd", 17) ;
    if (!openwritenclose_unsafe(fn, fmt, l)) strerr_diefu2sys(111, "write to ", fn) ;
  }
  memcpy(fn + dirlen, "/run", 5) ;
  fd = open_trunc(fn) ;
  if (fd == -1) strerr_diefu2sys(111, "open ", fn) ;
  buffer_init(&b, &buffer_write, fd, buf, 4096) ;
  if (!(*f)(&b, data)) strerr_diefu2sys(111, "write to ", fn) ;
  fd_close(fd) ;
  if (logger)
  {
    memcpy(fn + dirlen + 1, "type", 5) ;
    if (!openwritenclose_unsafe(fn, "longrun\n", 8)) strerr_diefu2sys(111, "write to ", fn) ;
    if (logger[0])
    {
      struct iovec v[2] = { { .iov_base = (char *)logger, .iov_len = strlen(logger) }, { .iov_base = "\n", .iov_len = 1 } } ;
      memcpy(fn + dirlen + 1, "producer-for", 13) ;
      if (!openwritevnclose_unsafe(fn, v, 2)) strerr_diefu2sys(111, "write to ", fn) ;
    }
  }
  else
  {
    if (chmod(fn, (~m & 0666) | ((~m >> 2) & 0111)) < 0)
      strerr_diefu2sys(111, "chmod ", fn) ;
    if (!(~m & 0400))
      strerr_warnw2x("weird umask, check permissions manually on ", fn) ;
  }
}
