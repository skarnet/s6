/* ISC license. */

#include <skalibs/bytestr.h>
#include <skalibs/djbunix.h>
#include <s6/s6-supervise.h>

int s6_svstatus_write (char const *dir, s6_svstatus_t const *status)
{
  unsigned int n = str_len(dir) ;
  char pack[S6_SVSTATUS_SIZE] ;
  char tmp[n + 2 + sizeof(S6_SVSTATUS_FILENAME)] ;
  byte_copy(tmp, n, dir) ;
  byte_copy(tmp + n, 2 + sizeof(S6_SVSTATUS_FILENAME), "/" S6_SVSTATUS_FILENAME) ;
  s6_svstatus_pack(pack, status) ;
  return openwritenclose_suffix(tmp, pack, S6_SVSTATUS_SIZE, ".new") ;
}
