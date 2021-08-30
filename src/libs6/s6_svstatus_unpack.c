/* ISC license. */

#include <stdint.h>
#include <skalibs/uint16.h>
#include <skalibs/uint64.h>
#include <skalibs/tai.h>
#include <s6/supervise.h>

void s6_svstatus_unpack (char const *pack, s6_svstatus_t *sv)
{
  uint64_t pid ;
  uint16_t wstat ;
  tain_unpack(pack, &sv->stamp) ;
  tain_unpack(pack + 12, &sv->readystamp) ;
  uint64_unpack_big(pack + 24, &pid) ;
  sv->pid = pid ;
  uint16_unpack_big(pack + 32, &wstat) ;
  sv->wstat = wstat ;
  sv->flagpaused = pack[34] & 1 ;
  sv->flagfinishing = !!(pack[34] & 2) ;
  sv->flagwantup = !!(pack[34] & 4) ;
  sv->flagready = !!(pack[34] & 8) ;
}
