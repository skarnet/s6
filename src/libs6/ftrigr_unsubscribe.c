/* ISC license. */

#include <errno.h>
#include <skalibs/uint16.h>
#include <skalibs/stralloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/textclient.h>
#include <s6/ftrigr.h>

int ftrigr_unsubscribe (ftrigr_t *a, uint16_t i, tain const *deadline, tain *stamp)
{
  ftrigr1_t *p ;
  char pack[3] = "--U" ;
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
  uint16_pack_big(pack, i) ;
  if (!textclient_command(&a->connection, pack, 3, deadline, stamp)) return 0 ;
  stralloc_free(&p->what) ;
  *p = ftrigr1_zero ;
  return gensetdyn_delete(&a->data, i) ;
}
