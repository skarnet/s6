/* ISC license. */

#include <errno.h>
#include <skalibs/error.h>
#include <skalibs/uint16.h>
#include <skalibs/genalloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>
#include <s6/s6lock.h>

static int msghandler (unixmessage_t const *m, void *context)
{
  s6lock_t *a = (s6lock_t *)context ;
  char *p ;
  uint16 id ;
  if (m->len != 3 || m->nfds) return (errno = EPROTO, 0) ;
  uint16_unpack_big(m->s, &id) ;
  p = GENSETDYN_P(char, &a->data, id) ;
  if (*p == EBUSY) *p = m->s[2] ;
  else if (error_isagain(*p)) *p = m->s[2] ? m->s[2] : EBUSY ;
  else return (errno = EPROTO, 0) ;
  if (!genalloc_append(uint16, &a->list, &id)) return 0 ;
  return 1 ;
}

int s6lock_update (s6lock_t *a)
{
  genalloc_setlen(uint16, &a->list, 0) ;
  return skaclient_update(&a->connection, &msghandler, a) ;
}
