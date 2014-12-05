/* ISC license. */

#include <skalibs/uint32.h>
#include <skalibs/tai.h>
#include <s6/s6-supervise.h>

void s6_svstatus_unpack (char const *pack, s6_svstatus_t_ref sv)
{
  uint32 pid ;
  tain_unpack(pack, &sv->stamp) ;
  uint32_unpack(pack + 12, &pid) ;
  sv->pid = (int)pid ;
  sv->flagpaused = pack[16] & 1 ;
  sv->flagfinishing = (pack[16] >> 1) & 1 ;
  switch (pack[17])
  {
    case 'u' :
      sv->flagwant = 1 ;
      sv->flagwantup = 1 ;
      break ;
    case 'd' :
      sv->flagwant = 1 ;
      sv->flagwantup = 0 ;
      break ;
    default :
      sv->flagwant = 0 ;
      sv->flagwantup = 0 ;
  }
}
