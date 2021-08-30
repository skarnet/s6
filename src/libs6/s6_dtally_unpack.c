/* ISC license. */

#include <skalibs/tai.h>
#include <s6/supervise.h>

void s6_dtally_unpack (char const *pack, s6_dtally_t *d)
{
  tain_unpack(pack, &d->stamp) ;
  d->exitcode = pack[TAIN_PACK] ;
  d->sig = pack[TAIN_PACK + 1] ;
}
