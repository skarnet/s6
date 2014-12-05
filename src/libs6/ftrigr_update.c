/* ISC license. */

#include <errno.h>
#include <skalibs/error.h>
#include <skalibs/uint16.h>
#include <skalibs/genalloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>
#include <s6/ftrigr.h>

static int msghandler (unixmessage_t const *m, void *context)
{
  ftrigr_t *a = (ftrigr_t *)context ;
  ftrigr1_t *p ;
  uint16 id ;
  if (m->len != 4 || m->nfds) return (errno = EPROTO, 0) ;
  uint16_unpack_big(m->s, &id) ;
  p = GENSETDYN_P(ftrigr1_t, &a->data, id) ;
  if (!p) return 1 ;
  if (p->state != FR1STATE_LISTENING) return (errno = EINVAL, 0) ;
  if (!genalloc_readyplus(uint16, &a->list, 1)) return 0 ;
  switch (m->s[2])
  {
    case 'd' :
      p->state = FR1STATE_WAITACK ;
      break ;
    case '!' :
      if (p->options & FTRIGR_REPEAT) p->count++ ;
      else p->state = FR1STATE_WAITACKDATA ;
      break ;
    default : return (errno = EPROTO, 0) ;
  }
  p->what = m->s[3] ;
  id++ ; genalloc_append(uint16, &a->list, &id) ;
  return 1 ;
}

int ftrigr_update (ftrigr_t *a)
{
  genalloc_setlen(uint16, &a->list, 0) ;
  return skaclient_update(&a->connection, &msghandler, a) ;
}
