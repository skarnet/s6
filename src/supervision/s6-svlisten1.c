/* ISC license. */

#include <stdint.h>
#include <signal.h>

#include <skalibs/sgetopt.h>
#include <skalibs/types.h>
#include <skalibs/sig.h>
#include <skalibs/tai.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>

#include "s6-svlisten.h"

#define USAGE "s6-svlisten1 [ -U | -u | -d | -D | -r | -R ] [ -t timeout ] servicedir prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  s6_svlisten_t foo = S6_SVLISTEN_ZERO ;
  tain_t deadline, tto ;
  pid_t pid ;
  int spfd ;
  int e ;
  int wantup = 1, wantready = 0, wantrestart = 0 ;
  uint16_t id ;
  unsigned char upstate, readystate ;
  PROG = "s6-svlisten1" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    unsigned int t = 0 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "uUdDrRt:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'u' : wantup = 1 ; wantrestart = 0 ; wantready = 0 ; break ;
        case 'U' : wantup = 1 ; wantrestart = 0 ; wantready = 1 ; break ;
        case 'd' : wantup = 0 ; wantrestart = 0 ; wantready = 0 ; break ;
        case 'D' : wantup = 0 ; wantrestart = 0 ; wantready = 1 ; break ;
        case 'r' : wantup = 1 ; wantrestart = 1 ; wantready = 0 ; break ;
        case 'R' : wantup = 1 ; wantrestart = 1 ; wantready = 1 ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&tto, t) ; else tto = tain_infinite_relative ;
  }
  if (argc < 2) dieusage() ;
  tain_now_set_stopwatch_g() ;
  tain_add_g(&deadline, &tto) ;
  spfd = s6_svlisten_selfpipe_init() ;
  s6_svlisten_init(1, argv, &foo, &id, &upstate, &readystate, &deadline) ;
  sig_restore(SIGPIPE) ;
  pid = child_spawn0(argv[1], argv + 1, envp) ;
  if (!pid) strerr_diefu2sys(111, "spawn ", argv[1]) ;
  if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "ignore SIGPIPE") ;
  if (wantrestart) s6_svlisten_loop(&foo, 0, 1, 1, &deadline, spfd, &s6_svlisten_signal_handler) ;
  e = s6_svlisten_loop(&foo, wantup, wantready, 1, &deadline, spfd, &s6_svlisten_signal_handler) ;
  if (e < 0) strerr_dief1x(102, "supervisor died") ;
  else if (e > 0) strerr_dief2x(1, argv[0], " failed permanently: the finish script exited 125") ;
  return 0 ;
}
