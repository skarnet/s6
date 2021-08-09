/* ISC license. */

#include <sys/types.h>
#include <sys/uio.h>
#include <stdint.h>
#include <errno.h>

#include <skalibs/gccattributes.h>
#include <skalibs/uint16.h>
#include <skalibs/genalloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/textclient.h>
#include <s6/ftrigr.h>

#include <skalibs/posixishard.h>

static inline int appears (uint16_t, uint16_t const *, size_t) gccattr_pure ;

static inline int appears (uint16_t id, uint16_t const *list, size_t len)
{
  while (len) if (id == list[--len]) return 1 ;
  return 0 ;
}

static int msghandler (struct iovec const *v, void *context)
{
  ftrigr_t *a = (ftrigr_t *)context ;
  ftrigr1_t *p ;
  int addit = 1 ;
  char const *s = v->iov_base ;
  uint16_t id ;
  if (v->iov_len != 4) return (errno = EPROTO, 0) ;
  uint16_unpack_big(s, &id) ;
  p = GENSETDYN_P(ftrigr1_t, &a->data, id) ;
  if (!p) return 1 ;
  if (p->state != FR1STATE_LISTENING) return (errno = EINVAL, 0) ;
  if (!genalloc_readyplus(uint16_t, &a->list, 1)) return 0 ;
  switch (s[2])
  {
    case 'd' :
      if (!stralloc_catb(&p->what, s + 3, 1)) return 0 ;
      p->state = FR1STATE_WAITACK ;
      break ;
    case '!' :
      if (!stralloc_catb(&p->what, s + 3, 1)) return 0 ;
      if (p->options & FTRIGR_REPEAT)
      {
        if (p->what.len > 1
         && appears(id+1, genalloc_s(uint16_t, &a->list), genalloc_len(uint16_t, &a->list)))
          addit = 0 ;
      }
      else p->state = FR1STATE_WAITACKDATA ;
      break ;
    default : return (errno = EPROTO, 0) ;
  }
  if (addit)
  {
    id++ ; genalloc_append(uint16_t, &a->list, &id) ;
  }
  return 1 ;
}

int ftrigr_updateb (ftrigr_t *a)
{
  size_t curlen = genalloc_len(uint16_t, &a->list) ;
  int r = textclient_update(&a->connection, &msghandler, a) ;
  return r < 0 ? r : (int)(genalloc_len(uint16_t, &a->list) - curlen) ;
}
