 /* ISC license. */

#include <errno.h>
#include <skalibs/uint32.h>
#include <skalibs/bytestr.h>
#include <skalibs/error.h>
#include <skalibs/stralloc.h>
#include <skalibs/unixmessage.h>
#include <s6/s6-fdholder.h>

int s6_fdholder_list_cb (unixmessage_t const *m, void *p)
{
  uint32 n ;
  register s6_fdholder_list_result_t *res = p ;
  if (m->nfds) goto droperr ;
  if (m->len < 5) goto err ;
  if (m->s[0])
  {
    res->err = m->s[0] ;
    return 1 ;
  }
  uint32_unpack_big(m->s + 1, &n) ;
  if (byte_count(m->s + 5, m->len - 5, 0) != n) goto err ;
  if (!stralloc_catb(res->sa, m->s + 5, m->len - 5)) return 0 ;
  res->n = n ;
  res->err = 0 ;
  return 1 ;

 droperr:
  unixmessage_drop(m) ;
 err:
  return (errno = EPROTO, 0) ;
}
