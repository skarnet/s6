/* ISC license. */

#include <stdint.h>
#include <skalibs/genalloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/textclient.h>
#include <s6/lock.h>

void s6lock_end (s6lock_t *a)
{
  gensetdyn_free(&a->data) ;
  genalloc_free(uint16_t, &a->list) ;
  textclient_end(&a->connection) ;
  *a = s6lock_zero ;
}
