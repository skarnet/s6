/* ISC license. */

#include <errno.h>

#include <skalibs/strerr.h>
#include <skalibs/djbunix.h>

#include "s6lockd.h"

int s6lockd_openandlock (char const *file, int ex, int nb)
{
  int fd, r ;
  if (ex)
  {
    fd = open_create(file) ;
    if (fd < 0) strerr_diefu3sys(111, "open ", file, " for writing") ;
  }
  else
  {
    fd = open_read(file) ;
    if (fd < 0)
    {
      if (errno != ENOENT) strerr_diefu3sys(111, "open ", file, " for reading") ;
      fd = open_create(file) ;
      if (fd < 0) strerr_diefu2sys(111, "create ", file) ;
      fd_close(fd) ;
      fd = open_read(file) ;
      if (fd < 0) strerr_diefu3sys(111, "open ", file, " for reading") ;
    }
  }
  r = fd_lock(fd, ex, nb) ;
  if (!r) errno = EBUSY ;
  if (r < 1) strerr_diefu2sys(1, "lock ", file) ;
  return fd ;
}
