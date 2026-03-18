/* ISC license. */

#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <skalibs/posixplz.h>
#include <skalibs/direntry.h>
#include <skalibs/djbunix.h>

#include <s6/ftrigw.h>

int ftrigw_clean (char const *path)
{
  size_t pathlen = strlen(path) ;
  DIR *dir = opendir(path) ;
  if (!dir) return 0 ;
  {
    char tmp[pathlen + 41] ;
    memcpy(tmp, path, pathlen) ;
    tmp[pathlen] = '/' ; tmp[pathlen + 40] = 0 ;
    for (;;)
    {
      direntry *d ;
      int fd ;
      errno = 0 ;
      d = readdir(dir) ;
      if (!d) break ;
      if (strncmp(d->d_name, "ftrig1", 6)) continue ;
      if (strlen(d->d_name) != 39) continue ;
      memcpy(tmp + pathlen + 1, d->d_name, 39) ;
      fd = open_write(tmp) ;
      if (fd >= 0) fd_close(fd) ;
      else if (errno == ENXIO) unlink_void(tmp) ;
    }
  }
  dir_close(dir) ;
  return !errno ;
}
