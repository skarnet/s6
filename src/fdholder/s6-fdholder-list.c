/* ISC license. */

#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <s6/config.h>

#define USAGE "s6-fdholder-list [ -t timeout ] [ -T ] socket"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  char const *newargv[9] ;
  unsigned int m = 0 ;
  unsigned int timeout = 0 ;
  int printexpire = 0 ;
  char fmtt[UINT_FMT] ;
  PROG = "s6-fdholder-list" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "d:t:T:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'T' : printexpire = 1 ; break ;
        case 't' : if (!uint0_scan(l.arg, &timeout)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) dieusage() ;

  newargv[m++] = S6_BINPREFIX "s6-ipcclient" ;
  newargv[m++] = "-l0" ;
  newargv[m++] = "--" ;
  newargv[m++] = argv[0] ;
  newargv[m++] = S6_BINPREFIX "s6-fdholder-listc" ;
  if (printexpire) newargv[m++] = "-T" ;
  if (timeout)
  {
    fmtt[uint_fmt(fmtt, timeout)] = 0 ;
    newargv[m++] = "-t" ;
    newargv[m++] = fmtt ;
  }
  newargv[m++] = 0 ;
  xpathexec_run(newargv[0], newargv, envp) ;
}
