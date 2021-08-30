/* ISC license. */

#include <errno.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/djbunix.h>
#include <s6/supervise.h>

int s6_svc_write (char const *fifo, char const *data, size_t datalen)
{
  int fd = open_write(fifo) ;
  if (fd < 0) switch (errno)
  {
    case ENXIO : return 0 ;
    case ENOENT :
    case ENOTDIR :
    case EISDIR : return -2 ;
    default : return -1 ;
  }
  if (ndelay_off(fd) == -1) return -1 ;
  if (datalen && fd_write(fd, data, datalen) == -1)
  {
    fd_close(fd) ;
    return -1 ;
  }
  fd_close(fd) ;
  return 1 ;
}
