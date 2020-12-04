/* ISC license. */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include <skalibs/posixplz.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/djbunix.h>

#include <s6/s6-supervise.h>

#ifdef PATH_MAX
# define S6_PATH_MAX PATH_MAX
#else
# define S6_PATH_MAX 4096
#endif

void s6_supervise_unlink (char const *scandir, char const *name, uint32_t options)
{
  int e = errno ;
  int fd = -1 ;
  size_t scandirlen = strlen(scandir) ;
  size_t namelen = strlen(name) ;
  char fn[scandirlen + namelen + sizeof(S6_SUPERVISE_CTLDIR) + 10] ;
  memcpy(fn, scandir, scandirlen) ;
  fn[scandirlen] = '/' ;
  memcpy(fn + scandirlen + 1, name, namelen) ;
  if (options & 1)
  {
    fn[scandirlen + 1 + namelen] = '/' ;
    memcpy(fn + scandirlen + 1 + namelen + 1, S6_SUPERVISE_CTLDIR, sizeof(S6_SUPERVISE_CTLDIR) - 1) ;
    memcpy(fn + scandirlen + 1 + namelen + sizeof(S6_SUPERVISE_CTLDIR), "/control", 9) ;
    fd = open_write(fn) ;
  }
  fn[scandirlen + 1 + namelen] = 0 ;
  unlink_void(fn) ;
  if (fd >= 0)
  {
    fd_write(fd, "xd", 1 + !!(options & 2)) ;
    fd_close(fd) ;
  }
  errno = e ;
}
