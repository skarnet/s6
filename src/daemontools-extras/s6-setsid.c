/* ISC license. */

#include <unistd.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>

#define USAGE "s6-setsid [ -i | -I ] prog..."

int main (int argc, char const *const *argv, char const *const *envp)
{
  int insist = 0 ;
  PROG = "s6-setsid" ;
  for (;;)
  {
    register int opt = subgetopt(argc, argv, "iI") ;
    if (opt == -1) break ;
    switch (opt)
    {
      case 'i' : insist = 1 ; break ;
      case 'I' : insist = 0 ; break ;
      default : strerr_dieusage(100, USAGE) ;
    }
  }
  argc -= subgetopt_here.ind ; argv += subgetopt_here.ind ;
  if (!argc) strerr_dieusage(100, USAGE) ;

  if (setsid() < 0)
  {
    if (insist) strerr_diefu1sys(111, "setsid") ;
    else strerr_warnwu1sys("setsid") ;
  }
  pathexec_run(argv[0], argv, envp) ;
  strerr_dieexec(111, argv[0]) ;
}
