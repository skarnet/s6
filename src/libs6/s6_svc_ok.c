/* ISC license. */

#include <string.h>
#include <errno.h>
#include <skalibs/djbunix.h>
#include <s6/s6-supervise.h>

int s6_svc_ok (char const *dir)
{
  size_t dirlen = strlen(dir) ;
  int fd ;
  char fn[dirlen + 9 + sizeof(S6_SUPERVISE_CTLDIR)] ;
  memcpy(fn, dir, dirlen) ;
  fn[dirlen] = '/' ;
  memcpy(fn + dirlen + 1, S6_SUPERVISE_CTLDIR, sizeof(S6_SUPERVISE_CTLDIR) - 1) ;
  memcpy(fn + dirlen + sizeof(S6_SUPERVISE_CTLDIR), "/control", 9) ;
  fd = open_write(fn) ;
  if (fd < 0)
  {
    if ((errno == ENXIO) || (errno == ENOENT)) return 0 ;
    else return -1 ;
  }
  fd_close(fd) ;
  return 1 ;
}
