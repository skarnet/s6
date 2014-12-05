/* ISC license. */

#include <skalibs/uint32.h>
#include <skalibs/tai.h>
#include <s6/s6-supervise.h>

void s6_svstatus_pack (char *pack, s6_svstatus_t const *sv)
{
  tain_pack(pack, &sv->stamp) ;
  uint32_pack(pack + 12, (uint32)sv->pid) ;
  pack[16] = sv->flagpaused | (sv->flagfinishing << 1) ;
  pack[17] = sv->flagwant ? sv->flagwantup ? 'u' : 'd' : 0 ;
}
