/* ISC license. */

#include <sys/types.h>
#include <skalibs/uint.h>
#include <skalibs/sgetopt.h>
#include <skalibs/env.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <execline/config.h>
#include <s6/config.h>

#define USAGE "s6-fdholder-transferdump [ -t timeoutfrom:timeoutto ] socketfrom socketto"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  char const *newargv[24] ;
  unsigned int timeoutfrom = 0, timeoutto = 0 ;
  unsigned int m = 0 ;
  char fmtfrom[UINT_FMT] ;
  char fmtto[UINT_FMT] ;
  PROG = "s6-fdholder-setdump" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "t:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 't' :
        {
          size_t pos = uint_scan(l.arg, &timeoutfrom) ;
          if (!pos)
          {
            if (l.arg[pos] != ':') dieusage() ;
            timeoutfrom = 0 ;
          }
          if (!l.arg[pos]) timeoutto = 0 ;
          else
          {
            if (l.arg[pos++] != ':') dieusage() ;
            if (!l.arg[pos]) timeoutto = 0 ;
            else if (!uint0_scan(l.arg + pos, &timeoutto)) dieusage() ;
          }
          break ;
        }
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
  newargv[m++] = EXECLINE_EXTBINPREFIX "fdclose" ;
  newargv[m++] = "7" ;
  newargv[m++] = EXECLINE_EXTBINPREFIX "fdmove" ;
  newargv[m++] = "0" ;
  newargv[m++] = "6" ;
  newargv[m++] = S6_BINPREFIX "s6-ipcclient" ;
  newargv[m++] = "-l0" ;
  newargv[m++] = "--" ;
  newargv[m++] = argv[1] ;
  newargv[m++] = EXECLINE_EXTBINPREFIX "fdclose" ;
  newargv[m++] = "6" ;
  newargv[m++] = EXECLINE_EXTBINPREFIX "fdmove" ;
  newargv[m++] = "1" ;
  newargv[m++] = "7" ;
  newargv[m++] = S6_BINPREFIX "s6-fdholder-transferdumpc" ;
  if (timeoutfrom)
  {
    fmtfrom[uint_fmt(fmtfrom, timeoutfrom)] = 0 ;
    newargv[m++] = "-t" ;
    newargv[m++] = fmtfrom ;
  }
  if (timeoutto)
  {
    fmtto[uint_fmt(fmtto, timeoutto)] = 0 ;
    newargv[m++] = "-T" ;
    newargv[m++] = fmtto ;
  }
  newargv[m++] = 0 ;
  pathexec_run(newargv[0], newargv, envp) ;
  strerr_dieexec(111, newargv[0]) ;
}
