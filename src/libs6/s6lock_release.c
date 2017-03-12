/* ISC license. */

#include <stdint.h>
#include <errno.h>
#include <skalibs/error.h>
#include <skalibs/types.h>
#include <skalibs/tai.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/skaclient.h>
#include <s6/s6lock.h>

int s6lock_release (s6lock_t *a, uint16_t i, tain_t const *deadline, tain_t *stamp)
{
  char *p = GENSETDYN_P(char, &a->data, i) ;
  if ((*p != EBUSY) && !error_isagain(*p))
  {
    s6lock_check(a, i) ;
    return 1 ;
  }
  {
    char err ;
    char pack[3] = "-->" ;
    uint16_pack_big(pack, i) ;
    if (!skaclient_send(&a->connection, pack, 3, &skaclient_default_cb, &err, deadline, stamp)) return 0 ;
    if (err) return (errno = err, 0) ;
  }
  *p = EINVAL ;
  return gensetdyn_delete(&a->data, i) ;
}
