/* ISC license. */

#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include <skalibs/posixplz.h>
#include <skalibs/tai.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include "ftrig1.h"

int ftrig1_make (ftrig1_t *f, char const *path)
{
  ftrig1_t ff = FTRIG1_ZERO ;
  size_t pathlen = strlen(path) ;
  char tmp[pathlen + FTRIG1_PREFIXLEN + 36] ;
  
  memcpy(tmp, path, pathlen) ;
  tmp[pathlen] = '/' ; tmp[pathlen+1] = '.' ;
  memcpy(tmp + pathlen + 2, FTRIG1_PREFIX, FTRIG1_PREFIXLEN) ;
  tmp[pathlen + 2 + FTRIG1_PREFIXLEN] = ':' ;
  if (!timestamp(tmp + pathlen + 3 + FTRIG1_PREFIXLEN)) return 0 ;
  memcpy(tmp + pathlen + FTRIG1_PREFIXLEN + 28, ":XXXXXX", 8) ;
  ff.fd = mkptemp2(tmp, O_NONBLOCK|O_CLOEXEC) ;
  if (ff.fd == -1) return 0 ;
  ff.fdw = open_write(tmp) ;
  if (ff.fdw == -1) goto err1 ;
  if (!stralloc_ready(&ff.name, pathlen + FTRIG1_PREFIXLEN + 36)) goto err2 ;
  stralloc_copyb(&ff.name, tmp, pathlen + 1) ;
  stralloc_catb(&ff.name, tmp + pathlen + 2, FTRIG1_PREFIXLEN + 34) ;
  if (rename(tmp, ff.name.s) == -1) goto err3 ;
  *f = ff ;
  return 1 ;

 err3:
  stralloc_free(&ff.name) ;
 err2:
  fd_close(ff.fdw) ;
 err1:
  fd_close(ff.fd) ;
  unlink_void(tmp) ;
  return 0 ;
}
