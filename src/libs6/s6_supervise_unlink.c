/* ISC license. */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <skalibs/posixplz.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/djbunix.h>

#include <s6/supervise.h>

void s6_supervise_unlink (char const *scdir, char const *name, uint32_t options)
{
  int e = errno ;
  int fd = -1, fdlog = -1 ;
  size_t scdirlen = strlen(scdir) ;
  size_t namelen = strlen(name) ;
  char fn[scdirlen + namelen + sizeof(S6_SUPERVISE_CTLDIR) + 14] ;
  memcpy(fn, scdir, scdirlen) ;
  fn[scdirlen] = '/' ;
  memcpy(fn + scdirlen + 1, name, namelen) ;
  if (options & 4)
  {
    memcpy(fn + scdirlen + 1 + namelen, "/down", 6) ;
    unlink_void(fn) ;
  }
  if (options & 1)
  {
    memcpy(fn + scdirlen + 1 + namelen, "/" S6_SUPERVISE_CTLDIR, sizeof(S6_SUPERVISE_CTLDIR)) ;
    memcpy(fn + scdirlen + 1 + namelen + sizeof(S6_SUPERVISE_CTLDIR), "/control", 9) ;
    fd = open_write(fn) ;
    memcpy(fn + scdirlen + 1 + namelen, "/log/" S6_SUPERVISE_CTLDIR, 4 + sizeof(S6_SUPERVISE_CTLDIR)) ;
    memcpy(fn + scdirlen + 5 + namelen + sizeof(S6_SUPERVISE_CTLDIR), "/control", 9) ;
    fdlog = open_write(fn) ;
  }
  
  fn[scdirlen + 1 + namelen] = 0 ;
  unlink_void(fn) ;
  if (fd >= 0)
  {
    fd_write(fd, "xd", 1 + !!(options & 2)) ;
    fd_close(fd) ;
  }
  if (fdlog >= 0)
  {
    fd_write(fdlog, "xo", 1 + !!(options & 2)) ;
    fd_close(fdlog) ;
  }
  errno = e ;
}
