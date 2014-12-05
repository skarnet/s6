/* ISC license. */

#include <skalibs/genalloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/skaclient.h>
#include <s6/s6lock.h>

void s6lock_end (s6lock_t *a)
{
  gensetdyn_free(&a->data) ;
  genalloc_free(uint16, &a->list) ;
  skaclient_end(&a->connection) ;
  *a = s6lock_zero ;
}
