/* ISC license. */

#include <errno.h>
#include <string.h>

#include <skalibs/djbunix.h>
#include <s6/supervise.h>

int s6_svstatus_read (char const *dir, s6_svstatus_t *status)
{
  ssize_t r ;
  size_t n = strlen(dir) ;
  char pack[S6_SVSTATUS_SIZE] ;
  char tmp[n + 1 + sizeof(S6_SVSTATUS_FILENAME)] ;
  memcpy(tmp, dir, n) ;
  memcpy(tmp + n, "/" S6_SVSTATUS_FILENAME, 1 + sizeof(S6_SVSTATUS_FILENAME)) ;
  r = openreadnclose(tmp, pack, S6_SVSTATUS_SIZE) ;
  if (r == -1) return 0 ;
  if (r < S6_SVSTATUS_SIZE) return (errno = EPIPE, 0) ;
  s6_svstatus_unpack(pack, status) ;
  return 1 ;
}
