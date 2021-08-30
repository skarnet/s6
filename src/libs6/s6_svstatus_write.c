/* ISC license. */

#include <string.h>
#include <skalibs/djbunix.h>
#include <s6/supervise.h>

int s6_svstatus_write (char const *dir, s6_svstatus_t const *status)
{
  size_t n = strlen(dir) ;
  char pack[S6_SVSTATUS_SIZE] ;
  char tmp[n + 1 + sizeof(S6_SVSTATUS_FILENAME)] ;
  memcpy(tmp, dir, n) ;
  memcpy(tmp + n, "/" S6_SVSTATUS_FILENAME, 1 + sizeof(S6_SVSTATUS_FILENAME)) ;
  s6_svstatus_pack(pack, status) ;
  return openwritenclose_suffix(tmp, pack, S6_SVSTATUS_SIZE, ".new") ;
}
