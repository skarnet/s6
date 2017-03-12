/* ISC license. */

#include <errno.h>
#include <skalibs/error.h>
#include <skalibs/unixmessage.h>
#include <s6/s6-fdholder.h>

int s6_fdholder_retrieve_cb (unixmessage_t const *m, void *p)
{
  s6_fdholder_retrieve_result_t *res = p ;
  if (m->len != 1) goto err ;
  if (m->s[0])
  {
    if (m->nfds) goto err ;
    res->err = m->s[0] ;
    return 1 ;
  }
  if (m->nfds != 1) goto err ;
  res->fd = m->fds[0] ;
  res->err = 0 ;
  return 1 ;

 err:
  unixmessage_drop(m) ;
  return (errno = EPROTO, 0) ;
}
