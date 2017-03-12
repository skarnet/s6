/* ISC license. */

#include <sys/types.h>
#include <errno.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/djbunix.h>
#include <s6/s6-supervise.h>

int s6_svc_write (char const *fifo, char const *data, size_t datalen)
{
  int fd = open_write(fifo) ;
  if (fd < 0) return (errno == ENXIO) ? 0 : -1 ;
  else if (ndelay_off(fd) == -1) return -1 ;
  else if (fd_write(fd, data, datalen) == -1)
  {
    int e = errno ;
    fd_close(fd) ;
    errno = e ;
    return -1 ;
  }
  fd_close(fd) ;
  return 1 ;
}
