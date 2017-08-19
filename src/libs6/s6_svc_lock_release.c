/* ISC license. */

#include <skalibs/djbunix.h>

void s6_svc_lock_release (int fd)
{
  fd_close(fd) ;
}
