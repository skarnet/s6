/* ISC license. */

#include <stdint.h>

#include <skalibs/sgetopt.h>
#include <skalibs/types.h>
#include <skalibs/bitarray.h>
#include <skalibs/tai.h>
#include <skalibs/strerr.h>
#include <skalibs/djbunix.h>

#include <s6/compat.h>
#include "s6-svlisten.h"

#define USAGE "s6-svlisten [ -U | -u | -d | -D | -r | -R ] [ -a | -o ] [ -t timeout ] servicedir... \"\" prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const **argv, char const *const *envp)
{
  tain deadline, tto ;
  int spfd ;
  int argc1 ;
  int or = 0 ;
  int wantup = 1, wantready = 0 ;
  PROG = "s6-svlisten" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    unsigned int t = 0 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "uUdDrRaot:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'u' : wantup = 1 ; wantready = 0 ; break ;
        case 'U' : wantup = 1 ; wantready = 1 ; break ;
        case 'd' : wantup = 0 ; wantready = 0 ; break ;
        case 'D' : wantup = 0 ; wantready = 1 ; break ;
        case 'r' : wantup = 2 ; wantready = 0 ; break ;
        case 'R' : wantup = 2 ; wantready = 1 ; break ;
        case 'a' : or = 0 ; break ;
        case 'o' : or = 1 ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&tto, t) ; else tto = tain_infinite_relative ;
  }
  if (argc < 3) dieusage() ;
  argc1 = s6_el_semicolon(argv) ;
  if (!argc1 || argc == argc1 + 1) dieusage() ;
  if (argc1 >= argc) strerr_dief1x(100, "unterminated servicedir block") ;
  if (wantup == 2 && or)
  {
    or = 0 ;
    strerr_warnw3x("-o is unsupported when combined with -", wantready ? "R" : "r", "- using -a instead") ;
  }

  tain_now_set_stopwatch_g() ;
  tain_add_g(&deadline, &tto) ;
  spfd = s6_svlisten_selfpipe_init() ;

  {
    s6_svlisten_t foo = S6_SVLISTEN_ZERO ;
    pid_t pid ;
    unsigned int e ;
    uint16_t ids[argc1] ;
    unsigned char upstate[bitarray_div8(argc1)] ;
    unsigned char readystate[bitarray_div8(argc1)] ;
    s6_svlisten_init(argc1, argv, &foo, ids, upstate, readystate, &deadline) ;
    pid = child_spawn0(argv[argc1 + 1], argv + argc1 + 1, envp) ;
    if (!pid) strerr_diefu2sys(111, "spawn ", argv[argc1 + 1]) ;
    if (wantup == 2)
    {
      wantup = 1 ;
      e = s6_svlisten_loop(&foo, 0, 1, or, &deadline, spfd, &s6_svlisten_signal_handler) ;
      if (e) strerr_dief1x(e, "some services reported permanent failure or their supervisor died") ;
    }
    e = s6_svlisten_loop(&foo, wantup, wantready, or, &deadline, spfd, &s6_svlisten_signal_handler) ;
    if (e) strerr_dief1x(e, "some services reported permanent failure or their supervisor died") ;
  }
  return 0 ;
}
