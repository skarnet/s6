/* ISC license. */

#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <s6/config.h>

#define USAGE "s6-fdholder-delete [ -t timeout ] socket id"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  char const *newargv[10] ;
  unsigned int timeout = 0 ;
  unsigned int m = 0 ;
  char fmtt[UINT_FMT] ;
  PROG = "s6-fdholder-delete" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "t:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 't' : if (!uint0_scan(l.arg, &timeout)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (argc < 2) dieusage() ;

  newargv[m++] = S6_BINPREFIX "s6-ipcclient" ;
  newargv[m++] = "-l0" ;
  newargv[m++] = "--" ;
  newargv[m++] = argv[0] ;
  newargv[m++] = S6_BINPREFIX "s6-fdholder-deletec" ;
  if (timeout)
  {
    fmtt[uint_fmt(fmtt, timeout)] = 0 ;
    newargv[m++] = "-t" ;
    newargv[m++] = fmtt ;
  }
  newargv[m++] = "--" ;
  newargv[m++] = argv[1] ;
  newargv[m++] = 0 ;
  xpathexec_run(newargv[0], newargv, envp) ;
}
