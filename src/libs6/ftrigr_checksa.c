/* ISC license. */

#include <errno.h>
#include <skalibs/stralloc.h>
#include <skalibs/gensetdyn.h>
#include <s6/ftrigr.h>

int ftrigr_checksa (ftrigr_t *a, uint16_t id, stralloc *sa)
{
  ftrigr1_t *p ;
  if (!id--) return (errno = EINVAL, -1) ;
  p = GENSETDYN_P(ftrigr1_t, &a->data, id) ;
  if (!p) return (errno = EINVAL, -1) ;
  switch (p->state)
  {
    case FR1STATE_WAITACKDATA :
    {
      if (!stralloc_catb(sa, p->what.s, p->what.len)) return -1 ;
      stralloc_free(&p->what) ;
      *p = ftrigr1_zero ;
      gensetdyn_delete(&a->data, id) ;
      return 1 ;
    }
    case FR1STATE_LISTENING :
    {
      if (!p->what.len) break ;
      if (!stralloc_catb(sa, p->what.s, p->what.len)) return -1 ;
      p->what.len = 0 ;
      return 1 ;
    }
    case FR1STATE_WAITACK :
    {
      int e ;
      if (!stralloc_catb(sa, p->what.s, p->what.len - 1)) return -1 ;
      e = p->what.s[p->what.len - 1] ;
      stralloc_free(&p->what) ;
      *p = ftrigr1_zero ;
      gensetdyn_delete(&a->data, id) ;
      errno = e ;
      return -1 ;
    }
    default: return (errno = EINVAL, -1) ;
  }
  return 0 ;
}
