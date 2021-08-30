/* ISC license. */

#include <string.h>
#include <skalibs/djbunix.h>
#include <s6/supervise.h>

int s6_dtally_write (char const *sv, s6_dtally_t const *tab, size_t n)
{
  size_t len = strlen(sv) ;
  char fn[len + sizeof(S6_DTALLY_FILENAME) + 1] ;
  char tmp[n ? S6_DTALLY_PACK * n : 1] ;
  memcpy(fn, sv, len) ;
  memcpy(fn + len, "/" S6_DTALLY_FILENAME, sizeof(S6_DTALLY_FILENAME) + 1) ;
  for (size_t i = 0 ; i < n ; i++) s6_dtally_pack(tmp + i * S6_DTALLY_PACK, tab + i) ;
  return openwritenclose_suffix(fn, tmp, n * S6_DTALLY_PACK, ".new") ;
}
