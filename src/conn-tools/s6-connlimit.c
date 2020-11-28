/* ISC license. */

#include <string.h>
#include <stdlib.h>

#include <skalibs/types.h>
#include <skalibs/strerr2.h>
#include <skalibs/exec.h>

int main (int argc, char const *const *argv)
{
  char const *x ;
  size_t protolen ;
  PROG = "s6-connlimit" ;
  x = getenv("PROTO") ;
  if (!x) strerr_dienotset(100, "PROTO") ;
  protolen = strlen(x) ;
  if (!protolen) strerr_dief1x(100, "empty PROTO") ;
  {
    unsigned int num ;
    char s[protolen + 8] ;
    memcpy(s, x, protolen) ;
    memcpy(s + protolen, "CONNNUM", 8) ;
    x = getenv(s) ;
    if (!x) strerr_dienotset(100, s) ;
    if (!uint0_scan(x, &num)) strerr_dief2x(100, "invalid ", s) ;
    memcpy(s + protolen + 4, "MAX", 4) ;
    x = getenv(s) ;
    if (x)
    {
      unsigned int max ;
      if (!uint0_scan(x, &max)) strerr_dief2x(100, "invalid ", s) ;
      if (num > max)
        strerr_dief2x(1, "number of connections from this client limited to ", x) ;
    }
  }
  (void)argc ;
  xexec0(argv+1) ;
}
