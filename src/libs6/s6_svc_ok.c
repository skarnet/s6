/* ISC license. */

#include <errno.h>
#include <skalibs/bytestr.h>
#include <skalibs/djbunix.h>
#include <s6/s6-supervise.h>

int s6_svc_ok (char const *dir)
{
  int fd ;
  unsigned int dirlen = str_len(dir) ;
  char fn[dirlen + 9 + sizeof(S6_SUPERVISE_CTLDIR)] ;
  byte_copy(fn, dirlen, dir) ;
  fn[dirlen] = '/' ;
  byte_copy(fn + dirlen + 1, sizeof(S6_SUPERVISE_CTLDIR) - 1, S6_SUPERVISE_CTLDIR) ;
  byte_copy(fn + dirlen + sizeof(S6_SUPERVISE_CTLDIR), 9, "/control") ;
  fd = open_write(fn) ;
  if (fd < 0)
  {
    if ((errno == ENXIO) || (errno == ENOENT)) return 0 ;
    else return -1 ;
  }
  fd_close(fd) ;
  return 1 ;
}
