/* ISC license. */

#include <sys/uio.h>
#include <stdint.h>
#include <errno.h>

#include <skalibs/posixishard.h>
#include <skalibs/error.h>
#include <skalibs/uint16.h>
#include <skalibs/genalloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/textclient.h>

#include <s6/s6lock.h>

static int msghandler (struct iovec const *v, void *context)
{
  s6lock_t *a = (s6lock_t *)context ;
  char const *s = v->iov_base ;
  char *p ;
  uint16_t id ;
  if (v->iov_len != 3) return (errno = EPROTO, 0) ;
  uint16_unpack_big(s, &id) ;
  p = GENSETDYN_P(char, &a->data, id) ;
  if (*p == EBUSY) *p = s[2] ;
  else if (error_isagain(*p)) *p = s[2] ? s[2] : EBUSY ;
  else return (errno = EPROTO, 0) ;
  if (!genalloc_append(uint16_t, &a->list, &id)) return 0 ;
  return 1 ;
}

int s6lock_update (s6lock_t *a)
{
  genalloc_setlen(uint16_t, &a->list, 0) ;
  return textclient_update(&a->connection, &msghandler, a) ;
}
