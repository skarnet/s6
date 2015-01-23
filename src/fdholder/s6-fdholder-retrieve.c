/* ISC license. */

#include <skalibs/uint.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <execline/config.h>
#include <s6/config.h>

#define USAGE "s6-fdholder-retrieve [ -D ] [ -t timeout ] socket id prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  unsigned int timeout = 0 ;
  int dodelete = 0 ;
  PROG = "s6-fdholder-retrieve" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "Dt:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'D' : dodelete = 1 ; break ;
        case 't' : if (!uint0_scan(l.arg, &timeout)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (argc < 3) dieusage() ;
  }

  {
    char const *newargv[13 + argc] ;
    unsigned int m = 0 ;
    char fmtt[UINT_FMT] ;
    newargv[m++] = S6_BINPREFIX "s6-ipcclient" ;
    newargv[m++] = "-l0" ;
    newargv[m++] = "--" ;
    newargv[m++] = *argv++ ;
    newargv[m++] = S6_BINPREFIX "s6-fdholder-retrievec" ;
    if (dodelete) newargv[m++] = "-D" ;
    if (timeout)
    {
      fmtt[uint_fmt(fmtt, timeout)] = 0 ;
      newargv[m++] = "-t" ;
      newargv[m++] = fmtt ;
    }
    newargv[m++] = "--" ;
    newargv[m++] = *argv++ ;
    newargv[m++] = EXECLINE_EXTBINPREFIX "fdclose" ;
    newargv[m++] = "6" ;
    newargv[m++] = EXECLINE_EXTBINPREFIX "fdclose" ;
    newargv[m++] = "7" ;
    while (*argv) newargv[m++] = *argv++ ;
    newargv[m++] = 0 ;
    pathexec_run(newargv[0], newargv, envp) ;
    strerr_dieexec(111, newargv[0]) ;
  }
}
