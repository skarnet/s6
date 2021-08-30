/* ISC license. */

#include <skalibs/tai.h>
#include <s6/supervise.h>

void s6_dtally_pack (char *pack, s6_dtally_t const *d)
{
  tain_pack(pack, &d->stamp) ;
  pack[TAIN_PACK] = d->exitcode ;
  pack[TAIN_PACK + 1] = d->sig ;
}
