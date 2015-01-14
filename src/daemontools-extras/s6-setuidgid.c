/* ISC license. */

#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <s6/config.h>

#define USAGE "s6-setuidgid username prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  char const *newargv[argc + 4] ;
  unsigned int m = 5 ;
  PROG = "s6-setuidgid" ;
  if (argc < 3) dieusage() ;
  argv++ ;
  newargv[0] = S6_BINPREFIX "s6-envuidgid" ;
  newargv[1] = *argv++ ;
  newargv[2] = S6_BINPREFIX "s6-applyuidgid" ;
  newargv[3] = "-Uz" ;
  newargv[4] = "--" ;
  while (*argv) newargv[m++] = *argv++ ;
  newargv[m++] = 0 ;
  pathexec_run(newargv[0], newargv, envp) ;
  strerr_dieexec(111, newargv[0]) ;
}
