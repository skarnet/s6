/* ISC license. */

#include <unistd.h>
#include <signal.h>

#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/sig.h>
#include <skalibs/exec.h>

#define USAGE "s6-setsid [ -s | -b | -f | -g ] [ -i | -I | -q ] [ -d ctty ] prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  unsigned int ctty = 0, what = 0, insist = 1 ;
  PROG = "s6-setsid" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "sbfgiIqd:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 's' : what = 0 ; break ;
        case 'b' : what = 1 ; break ;
        case 'f' : what = 2 ; break ;
        case 'g' : what = 3 ; break ;
        case 'i' : insist = 2 ; break ;
        case 'I' : insist = 1 ; break ;
        case 'q' : insist = 0 ; break ;
        case 'd' : if (!uint0_scan(l.arg, &ctty)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) dieusage() ;

  if (what)
  {
    if (setpgid(0, 0) < 0) switch (insist)
    {
      case 2 : strerr_diefu1sys(111, "setpgid") ;
      case 1 : strerr_warnwu1sys("setpgid") ; break ;
      default : break ;
    }

    if (what >= 2)
    {
      if (what == 3) sig_ignore(SIGTTOU) ;
      if (tcsetpgrp(ctty, getpid()) < 0) switch (insist)
      {
        case 2 : strerr_diefu1sys(111, "tcsetpgrp") ;
        case 1 : strerr_warnwu1sys("tcsetpgrp") ; break ;
        default : break ;
      }
      if (what == 3) sig_restore(SIGTTOU) ;
    }
  }
  else if (setsid() < 0) switch (insist)
  {
    case 2 : strerr_diefu1sys(111, "setsid") ;
    case 1 : strerr_warnwu1sys("setsid") ; break ;
    default : break ;
  }

  xexec(argv) ;
}
