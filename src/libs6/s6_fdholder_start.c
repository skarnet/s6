/* ISC license. */

#include <skalibs/djbunix.h>
#include <skalibs/webipc.h>
#include <skalibs/unixconnection.h>
#include <s6/s6-fdholder.h>

int s6_fdholder_start (s6_fdholder_t *a, char const *path, tain_t const *deadline, tain_t *stamp)
{
  int fd = ipc_stream_nb() ;
  if (fd < 0) return 0 ;
  if (!ipc_timed_connect(fd, path, deadline, stamp))
  {
    fd_close(fd) ;
    return 0 ;
  }
  s6_fdholder_init(a, fd) ;
  return 1 ;
}
