/* ISC license. */

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <skalibs/direntry.h>
#include <skalibs/djbunix.h>
#include "ftrig1.h"
#include <s6/ftrigw.h>

int ftrigw_clean (char const *path)
{
  size_t pathlen = strlen(path) ;
  int e = 0 ;
  DIR *dir = opendir(path) ;
  if (!dir) return 0 ;
  {
    char tmp[pathlen + FTRIG1_PREFIXLEN + 45] ;
    memcpy(tmp, path, pathlen) ;
    tmp[pathlen] = '/' ; tmp[pathlen + FTRIG1_PREFIXLEN + 44] = 0 ;
    for (;;)
    {
      direntry *d ;
      int fd ;
      errno = 0 ;
      d = readdir(dir) ;
      if (!d) break ;
      if (strncmp(d->d_name, FTRIG1_PREFIX, FTRIG1_PREFIXLEN)) continue ;
      if (strlen(d->d_name) != FTRIG1_PREFIXLEN + 43) continue ;
      memcpy(tmp + pathlen + 1, d->d_name, FTRIG1_PREFIXLEN + 43) ;
      fd = open_write(tmp) ;
      if (fd >= 0) fd_close(fd) ;
      else if ((errno == ENXIO) && (unlink(tmp) < 0)) e = errno ;
    }
  }
  dir_close(dir) ;
  if (errno) e = errno ;
  return e ? (errno = e, 0) : 1 ;
}
