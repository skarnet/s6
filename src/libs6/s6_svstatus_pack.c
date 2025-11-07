/* ISC license. */

#include <stdint.h>
#include <skalibs/uint16.h>
#include <skalibs/uint64.h>
#include <skalibs/tai.h>
#include <s6/supervise.h>

void s6_svstatus_pack (char *pack, s6_svstatus_t const *sv)
{
  tain_pack(pack, &sv->stamp) ;
  tain_pack(pack + 12, &sv->readystamp) ;
  uint64_pack_big(pack + 24, (uint64_t)sv->pid) ;
  uint16_pack_big(pack + 32, (uint16_t)sv->wstat) ;
  pack[34] =
    sv->flagpaused |
    (sv->flagfinishing << 1) |
    (sv->flagwantup << 2) |
    (sv->flagready << 3) ;
}
