 /* ISC license. */

#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/exec.h>

#include <s6/config.h>

#define USAGE "s6-sudo [ -q | -Q | -v ] [ -p bindpath ] [ -l localname ] [ -e ] [ -t timeout ] [ -T timeoutrun ] path [ args... ]"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  unsigned int verbosity = 1, t = 0, T = 0 ;
  char const *bindpath = 0 ;
  char const *localname = 0 ;
  int nodoenv = 0 ;
  PROG = "s6-sudo" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "qQvp:l:et:T:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'q' : if (verbosity) verbosity-- ; break ;
        case 'Q' : verbosity = 1 ; break ;
        case 'v' : verbosity++ ; break ;
        case 'p' : bindpath = l.arg ; break ;
        case 'l' : localname = l.arg ; break ;
        case 'e' : nodoenv = 1 ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        case 'T' : if (!uint0_scan(l.arg, &T)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) dieusage() ;
  if (verbosity > 4) verbosity = 4 ;
  {
    char const *eargv[9 + argc + ((verbosity < 2 ? 1 : verbosity-1)) + ((!!bindpath + !!localname) << 1) + nodoenv] ;
    char fmt1[UINT_FMT] ;
    char fmt2[UINT_FMT] ;
    unsigned int n = 0 ;
    eargv[n++] = S6_BINPREFIX "s6-ipcclient" ;
    if (!verbosity) eargv[n++] = "-Q" ;
    else while (--verbosity) eargv[n++] = "-v" ;
    if (bindpath) { eargv[n++] = "-p" ; eargv[n++] = bindpath ; }
    if (localname) { eargv[n++] = "-l" ; eargv[n++] = localname ; }
    eargv[n++] = "--" ;
    eargv[n++] = *argv++ ; argc-- ;
    eargv[n++] = S6_BINPREFIX "s6-sudoc" ;
    if (nodoenv) eargv[n++] = "-e" ;
    eargv[n++] = "-t" ;
    fmt1[uint_fmt(fmt1, t)] = 0 ;
    eargv[n++] = fmt1 ;
    eargv[n++] = "-T" ;
    fmt2[uint_fmt(fmt2, T)] = 0 ;
    eargv[n++] = fmt2 ;
    eargv[n++] = "--" ;
    while (argc--) eargv[n++] = *argv++ ;
    eargv[n++] = 0 ;
    xexec(eargv) ;
  }
}
