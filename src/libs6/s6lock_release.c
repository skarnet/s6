/* ISC license. */

#include <errno.h>
#include <skalibs/error.h>
#include <skalibs/uint16.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/textclient.h>
#include <s6/lock.h>

int s6lock_release (s6lock_t *a, uint16_t i, tain const *deadline, tain *stamp)
{
  unsigned char *p = GENSETDYN_P(unsigned char, &a->data, i) ;
  char pack[3] = "-->" ;
  if ((*p != EBUSY) && !error_isagain(*p))
  {
    s6lock_check(a, i) ;
    return 1 ;
  }
  uint16_pack_big(pack, i) ;
  if (!textclient_command(&a->connection, pack, 3, deadline, stamp)) return 0 ;
  *p = EINVAL ;
  return gensetdyn_delete(&a->data, i) ;
}
