/* ISC license. */

#include <string.h>
#include <skalibs/types.h>
#include <skalibs/strerr2.h>
#include <skalibs/env.h>
#include <skalibs/djbunix.h>

int main (int argc, char const *const *argv, char const *const *envp)
{
  char const *x ;
  size_t protolen ;
  PROG = "s6-connlimit" ;
  x = env_get2(envp, "PROTO") ;
  if (!x) strerr_dienotset(100, "PROTO") ;
  protolen = strlen(x) ;
  if (!protolen) strerr_dief1x(100, "empty PROTO") ;
  {
    unsigned int num ;
    char s[protolen + 8] ;
    memcpy(s, x, protolen) ;
    memcpy(s + protolen, "CONNNUM", 8) ;
    x = env_get2(envp, s) ;
    if (!x) strerr_dienotset(100, s) ;
    if (!uint0_scan(x, &num)) strerr_dief2x(100, "invalid ", s) ;
    memcpy(s + protolen + 4, "MAX", 4) ;
    x = env_get2(envp, s) ;
    if (x)
    {
      unsigned int max ;
      if (!uint0_scan(x, &max)) strerr_dief2x(100, "invalid ", s) ;
      if (num > max)
        strerr_dief2x(1, "number of connections from this client limited to ", x) ;
    }
  }
  pathexec0_run(argv+1, envp) ;
  (void)argc ;
  strerr_dieexec(111, argv[1]) ;
}
