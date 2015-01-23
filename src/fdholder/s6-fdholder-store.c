/* ISC license. */

#include <skalibs/uint.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <s6/config.h>

#define USAGE "s6-fdholder-store [ -d fd ] [ -t timeout ] [ -T fdtimeout ] socket id"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  char const *newargv[12] ;
  unsigned int timeout = 0, limit = 0 ;
  unsigned int m = 0 ;
  char fmtt[UINT_FMT] ;
  char fmtl[UINT_FMT] ;
  PROG = "s6-fdholder-store" ;
  {
    unsigned int fd = 0 ;
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "d:t:T:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'd' : if (!uint0_scan(l.arg, &fd)) dieusage() ; break ;
        case 't' : if (!uint0_scan(l.arg, &timeout)) dieusage() ; break ;
        case 'T' : if (!uint0_scan(l.arg, &limit)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (argc < 2) dieusage() ;
    if (fd && fd_move(0, fd) < 0)
      strerr_diefu1sys(111, "move file descriptor") ;
  }

  newargv[m++] = S6_BINPREFIX "s6-ipcclient" ;
  newargv[m++] = "-l0" ;
  newargv[m++] = "--" ;
  newargv[m++] = argv[0] ;
  newargv[m++] = S6_BINPREFIX "s6-fdholder-storec" ;
  if (timeout)
  {
    fmtt[uint_fmt(fmtt, timeout)] = 0 ;
    newargv[m++] = "-t" ;
    newargv[m++] = fmtt ;
  }
  if (limit)
  {
    fmtl[uint_fmt(fmtl, limit)] = 0 ;
    newargv[m++] = "-T" ;
    newargv[m++] = fmtl ;
  }
  newargv[m++] = "--" ;
  newargv[m++] = argv[1] ;
  newargv[m++] = 0 ;
  pathexec_run(newargv[0], newargv, envp) ;
  strerr_dieexec(111, newargv[0]) ;
}
