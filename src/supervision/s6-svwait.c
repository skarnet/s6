/* ISC license. */

#include <stdint.h>
#include <signal.h>

#include <skalibs/sgetopt.h>
#include <skalibs/types.h>
#include <skalibs/bitarray.h>
#include <skalibs/sig.h>
#include <skalibs/tai.h>
#include <skalibs/strerr.h>

#include "s6-svlisten.h"

#define USAGE "s6-svwait [ -U | -u | -d | -D | -r | -R ] [ -a | -o ] [ -t timeout ] servicedir..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  tain deadline ;
  int or = 0 ;
  int wantup = 1, wantready = 0 ;
  PROG = "s6-svwait" ;
  {
    unsigned int t = 0 ;
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "UudDrRaot:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'U' : wantup = 1 ; wantready = 1 ; break ;
        case 'u' : wantup = 1 ; wantready = 0 ; break ;
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
    if (t) tain_from_millisecs(&deadline, t) ; else deadline = tain_infinite_relative ;
  }
  if (!argc) return 0 ;
  tain_now_set_stopwatch_g() ;
  tain_add_g(&deadline, &deadline) ;

  {
    s6_svlisten_t foo = S6_SVLISTEN_ZERO ;
    unsigned int e ;
    uint16_t ids[argc] ;
    unsigned char upstate[bitarray_div8(argc)] ;
    unsigned char readystate[bitarray_div8(argc)] ;
    if (!sig_ignore(SIGPIPE)) strerr_diefu1sys(111, "ignore SIGPIPE") ;
    s6_svlisten_init(argc, argv, &foo, ids, upstate, readystate, &deadline) ;
    if (wantup == 2)
    {
      wantup = 1 ;
      e = s6_svlisten_loop(&foo, 0, 0, 0, &deadline, -1, 0) ;
      if (e) strerr_dief1x(e, "some services reported permanent failure or their supervisor died") ;
    }
    e = s6_svlisten_loop(&foo, wantup, wantready, or, &deadline, -1, 0) ;
    if (e) strerr_dief1x(e, "some services reported permanent failure or their supervisor died") ; 
  }
  return 0 ;
}
