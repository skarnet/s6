/* ISC license. */

#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <skalibs/posixplz.h>
#include <skalibs/direntry.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/djbunix.h>

#include <s6/ftrigw.h>
#include "ftrig1.h"

int ftrigw_notifyb_nosig (char const *path, char const *s, size_t len)
{
  unsigned int i = 0 ;
  DIR *dir = opendir(path) ;
  if (!dir) return -1 ;
  {
    size_t pathlen = strlen(path) ;
    char tmp[pathlen + FTRIG1_PREFIXLEN + 35] ;
    memcpy(tmp, path, pathlen) ;
    tmp[pathlen] = '/' ;
    tmp[pathlen + FTRIG1_PREFIXLEN + 34] = 0 ;
    for (;;)
    {
      direntry *d ;
      int fd ;
      errno = 0 ;
      d = readdir(dir) ;
      if (!d) break ;
      if (strncmp(d->d_name, FTRIG1_PREFIX ":@", FTRIG1_PREFIXLEN + 2)) continue ;
      if (strlen(d->d_name) != FTRIG1_PREFIXLEN + 33) continue ;
      memcpy(tmp + pathlen + 1, d->d_name, FTRIG1_PREFIXLEN + 33) ;
      fd = open_write(tmp) ;
      if (fd == -1)
      {
        if (errno == ENXIO) unlink_void(tmp) ;
      }
      else
      {
        ssize_t r = fd_write(fd, s, len) ;
        if ((r < 0) || (size_t)r < len)
        {
          if (errno == EPIPE) unlink_void(tmp) ;
           /* what to do if EAGAIN ? full fifo -> fix the reader !
              There's a race condition in extreme cases though ;
              but it's still better to be nonblocking - the writer
              shouldn't get in trouble because of a bad reader. */
          fd_close(fd) ;
        }
        else
        {
          fd_close(fd) ;
          i++ ;
        }
      }
    }
  }
  dir_close(dir) ;
  return errno ? -1 : (int)i ;
}
