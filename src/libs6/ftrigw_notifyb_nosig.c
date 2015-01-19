/* ISC license. */

#include <unistd.h>
#include <errno.h>
#include <skalibs/direntry.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/bytestr.h>
#include <skalibs/djbunix.h>
#include "ftrig1.h"
#include <s6/ftrigw.h>

int ftrigw_notifyb_nosig (char const *path, char const *s, unsigned int len)
{
  unsigned int i = 0 ;
  DIR *dir = opendir(path) ;
  if (!dir) return -1 ;
  {
    unsigned int pathlen = str_len(path) ;
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
      if (fd == -1)
      {
        if (errno == ENXIO) unlink(tmp) ;
      }
      else
      {
        register int r = fd_write(fd, s, len) ;
        if ((r < 0) || (unsigned int)r < len)
        {
          if (errno == EPIPE) unlink(tmp) ;
           /* what to do if EGAIN ? full fifo -> fix the reader !
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
  {
    int e = errno ;
    dir_close(dir) ;
    return e ? (errno = e, -1) : (int)i ;
  }
}
