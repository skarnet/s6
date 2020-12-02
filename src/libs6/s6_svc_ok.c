/* ISC license. */

#include <string.h>
#include <errno.h>

#include <skalibs/djbunix.h>

#include <s6/s6-supervise.h>

int s6_svc_ok (char const *dir)
{
  int r ;
  int e = errno ;
  int fd ;
  size_t dirlen = strlen(dir) ;
  char fn[dirlen + 6 + sizeof(S6_SUPERVISE_CTLDIR)] ;
  memcpy(fn, dir, dirlen) ;
  memcpy(fn + dirlen, "/" S6_SUPERVISE_CTLDIR "/lock", 6 + sizeof(S6_SUPERVISE_CTLDIR)) ;
  fd = open_read(fn) ;
  if (fd < 0) return errno == ENOENT ? (errno = e, 0) : -1 ;
  r = fd_islocked(fd) ;
  fd_close(fd) ;
  return r ;
}
