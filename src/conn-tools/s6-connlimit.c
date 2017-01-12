/* ISC license. */

#include <sys/types.h>
#include <skalibs/uint.h>
#include <skalibs/bytestr.h>
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
  protolen = str_len(x) ;
  if (!protolen) strerr_dief1x(100, "empty PROTO") ;
  {
    unsigned int num ;
    char s[protolen + 8] ;
    byte_copy(s, protolen, x) ;
    byte_copy(s + protolen, 8, "CONNNUM") ;
    x = env_get2(envp, s) ;
    if (!x) strerr_dienotset(100, s) ;
    if (!uint0_scan(x, &num)) strerr_dief2x(100, "invalid ", s) ;
    byte_copy(s + protolen + 4, 4, "MAX") ;
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
