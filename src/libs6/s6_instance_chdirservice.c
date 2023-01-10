/* ISC license. */

#include <string.h>
#include <unistd.h>

#include <skalibs/strerr.h>
#include <skalibs/djbunix.h>

#include <s6/supervise.h>

void s6_instance_chdirservice (char const *s)
{
  int fd, r ;
  size_t len = strlen(s) ;
  char fn[len + 10] ;
  if (!*s) strerr_dief1x(100, "invalid service path") ;
  memcpy(fn, s, len) ;
  memcpy(fn + len, "/instance", 10) ;
  if (chdir(fn) == -1) strerr_diefu2sys(111, "chdir to ", fn) ;
  fd = open_read(S6_SVSCAN_CTLDIR "/lock") ;
  if (fd < 0) strerr_diefu3sys(111, "open ", fn, "/" S6_SVSCAN_CTLDIR "/lock") ;
  r = fd_islocked(fd) ;
  if (r < 0) strerr_diefu3sys(111, "check lock on ", fn, "/" S6_SVSCAN_CTLDIR "/lock") ;
  if (!r) strerr_dief2x(1, "instanced service not running on ", s) ;
  fd_close(fd) ;
}
