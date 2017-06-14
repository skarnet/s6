/* ISC license. */

#include <errno.h>
#include <skalibs/stralloc.h>
#include <s6/ftrigr.h>

int ftrigr_check (ftrigr_t *a, uint16_t id, char *c)
{
  stralloc sa = STRALLOC_ZERO ;
  int r = ftrigr_checksa(a, id, &sa) ;

  if (r && sa.len)
  {
    int e = errno ;
    *c = sa.s[sa.len - 1] ;
    stralloc_free(&sa) ;
    errno = e ;
  }
  return r ;
}
