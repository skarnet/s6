/* ISC license. */

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <skalibs/direntry.h>
#include <skalibs/bytestr.h>
#include <skalibs/djbunix.h>
#include "ftrig1.h"
#include <s6/ftrigw.h>

int ftrigw_clean (char const *path)
{
  size_t pathlen = str_len(path) ;
  int e = 0 ;
  DIR *dir = opendir(path) ;
  if (!dir) return 0 ;
  {
    char tmp[pathlen + FTRIG1_PREFIXLEN + 45] ;
    byte_copy(tmp, pathlen, path) ;
    tmp[pathlen] = '/' ; tmp[pathlen + FTRIG1_PREFIXLEN + 44] = 0 ;
    for (;;)
    {
      direntry *d ;
      int fd ;
      errno = 0 ;
      d = readdir(dir) ;
      if (!d) break ;
      if (str_diffn(d->d_name, FTRIG1_PREFIX, FTRIG1_PREFIXLEN)) continue ;
      if (str_len(d->d_name) != FTRIG1_PREFIXLEN + 43) continue ;
      byte_copy(tmp + pathlen + 1, FTRIG1_PREFIXLEN + 43, d->d_name) ;
      fd = open_write(tmp) ;
      if (fd >= 0) fd_close(fd) ;
      else if ((errno == ENXIO) && (unlink(tmp) < 0)) e = errno ;
    }
  }
  if (errno) e = errno ;
  dir_close(dir) ;
  return e ? (errno = e, 0) : 1 ;
}
