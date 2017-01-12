/* ISC license. */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>      /* for rename() */
#include <errno.h>
#include <skalibs/bytestr.h>
#include <skalibs/tai.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/surf.h>
#include <skalibs/random.h>
#include "ftrig1.h"

static SURFSchedule surf_ctx = SURFSCHEDULE_ZERO ;

void ftrig1_init (void)
{
  char seed[160] ;
  random_makeseed(seed) ;
  surf_init(&surf_ctx, seed) ;
}

static inline void surfname (char *s, size_t n)
{
  static char const oklist[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZghijklmnopqrstuvwxyz-_0123456789abcdef" ;
  surf(&surf_ctx, s, n) ;
  while (n--) s[n] = oklist[s[n] & 63] ;
}

int ftrig1_make (ftrig1_t *f, char const *path)
{
  ftrig1_t ff = FTRIG1_ZERO ;
  size_t pathlen = str_len(path) ;
  int e = 0 ;
  char tmp[pathlen + 46 + FTRIG1_PREFIXLEN] ;
  
  byte_copy(tmp, pathlen, path) ;
  tmp[pathlen] = '/' ; tmp[pathlen+1] = '.' ;
  byte_copy(tmp + pathlen + 2, FTRIG1_PREFIXLEN, FTRIG1_PREFIX) ;
  tmp[pathlen + 2 + FTRIG1_PREFIXLEN] = ':' ;
  if (!timestamp(tmp + pathlen + 3 + FTRIG1_PREFIXLEN)) return 0 ;
  tmp[pathlen + 28 + FTRIG1_PREFIXLEN] = ':' ;
  surfname(tmp + pathlen + 29 + FTRIG1_PREFIXLEN, 16) ;
  tmp[pathlen + 45 + FTRIG1_PREFIXLEN] = 0 ;
  
  {
    mode_t m = umask(0) ;
    if (mkfifo(tmp, S_IRUSR|S_IWUSR|S_IWGRP|S_IWOTH) == -1)
    {
      umask(m) ;
      return 0 ;
    }
    umask(m) ;
  }

  if (!stralloc_catb(&ff.name, tmp, pathlen+1)) { e = errno ; goto err0 ; }
  if (!stralloc_catb(&ff.name, tmp + pathlen + 2, FTRIG1_PREFIXLEN + 44))
  {
    e = errno ; goto err1 ;
  }
  ff.fd = open_read(tmp) ;
  if (ff.fd == -1) { e = errno ; goto err1 ; }
  ff.fdw = open_write(tmp) ;
  if (ff.fdw == -1) { e = errno ; goto err2 ; }
  if (rename(tmp, ff.name.s) == -1) goto err3 ;
  *f = ff ;
  return 1 ;

 err3:
  e = errno ;
  fd_close(ff.fdw) ;
 err2:
  fd_close(ff.fd) ;
 err1:
  stralloc_free(&ff.name) ;
 err0:
  unlink(tmp) ;
  errno = e ;
  return 0 ;
}
