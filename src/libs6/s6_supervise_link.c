/* ISC license. */

#include <string.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <s6/supervise.h>

int s6_supervise_link (char const *scdir, char const *const *servicedirs, size_t n, char const *prefix, uint32_t options, tain const *deadline, tain *stamp)
{
  int r ;
  size_t prefixlen = strlen(prefix) ;
  stralloc sa = STRALLOC_ZERO ;
  char const *names[n ? n : 1] ;
  {
    size_t indices[n ? n : 1] ;
    for (size_t i = 0 ; i < n ; i++)
    {
      indices[i] = sa.len ;
      if (!stralloc_catb(&sa, prefix, prefixlen)
       || !sabasename(&sa, servicedirs[i], strlen(servicedirs[i]))
       || !stralloc_0(&sa)) goto err ;
    }
    for (size_t i = 0 ; i < n ; i++) names[i] = sa.s + indices[i] ;
  }
  r = s6_supervise_link_names(scdir, servicedirs, names, n, options, deadline, stamp) ;
  stralloc_free(&sa) ;
  return r ;

 err:
  stralloc_free(&sa) ;
  return -1 ;
}
