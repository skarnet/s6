/* ISC license. */

#include <stdint.h>
#include <errno.h>
#include <skalibs/uint16.h>
#include <skalibs/tai.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/skaclient.h>
#include <s6/ftrigr.h>

int ftrigr_unsubscribe (ftrigr_t *a, uint16_t i, tain_t const *deadline, tain_t *stamp)
{
  ftrigr1_t *p ;
  if (!i--) return (errno = EINVAL, 0) ;
  p = GENSETDYN_P(ftrigr1_t, &a->data, i) ;
  if (!p) return (errno = EINVAL, 0) ;
  switch (p->state)
  {
    case FR1STATE_WAITACK :
    case FR1STATE_WAITACKDATA :
    {
      char dummy ;
      ftrigr_check(a, i+1, &dummy) ;
      return 1 ;
    }
    default : break ;
  }
  {
    char err ;
    char pack[3] = "--U" ;
    uint16_pack_big(pack, i) ;
    if (!skaclient_send(&a->connection, pack, 3, &skaclient_default_cb, &err, deadline, stamp)) return 0 ;
    if (err) return (errno = err, 0) ;
  }
  *p = ftrigr1_zero ;
  return gensetdyn_delete(&a->data, i) ;
}
