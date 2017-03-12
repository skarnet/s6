/* ISC license. */

#include <sys/types.h>
#include <stdint.h>
#include <errno.h>
#include <skalibs/gccattributes.h>
#include <skalibs/error.h>
#include <skalibs/types.h>
#include <skalibs/genalloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>
#include <s6/ftrigr.h>

static inline int appears (uint16_t, uint16_t const *, size_t) gccattr_pure ;

static inline int appears (uint16_t id, uint16_t const *list, size_t len)
{
  while (len) if (id == list[--len]) return 1 ;
  return 0 ;
}

static int msghandler (unixmessage_t const *m, void *context)
{
  ftrigr_t *a = (ftrigr_t *)context ;
  ftrigr1_t *p ;
  int addit = 1 ;
  uint16_t id ;
  if (m->len != 4 || m->nfds) return (errno = EPROTO, 0) ;
  uint16_unpack_big(m->s, &id) ;
  p = GENSETDYN_P(ftrigr1_t, &a->data, id) ;
  if (!p) return 1 ;
  if (p->state != FR1STATE_LISTENING) return (errno = EINVAL, 0) ;
  if (!genalloc_readyplus(uint16_t, &a->list, 1)) return 0 ;
  switch (m->s[2])
  {
    case 'd' :
      p->state = FR1STATE_WAITACK ;
      break ;
    case '!' :
      if (p->options & FTRIGR_REPEAT)
      {
        if (p->count++
         && appears(id+1, genalloc_s(uint16_t, &a->list), genalloc_len(uint16_t, &a->list)))
          addit = 0 ;
      }
      else p->state = FR1STATE_WAITACKDATA ;
      break ;
    default : return (errno = EPROTO, 0) ;
  }
  p->what = m->s[3] ;
  if (addit)
  {
    id++ ; genalloc_append(uint16_t, &a->list, &id) ;
  }
  return 1 ;
}

int ftrigr_update (ftrigr_t *a)
{
  int r ;
  genalloc_setlen(uint16_t, &a->list, 0) ;
  r = skaclient_update(&a->connection, &msghandler, a) ;
  return r < 0 ? r : (int)genalloc_len(uint16_t, &a->list) ;
}
