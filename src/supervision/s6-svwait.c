/* ISC license. */

#include <stdint.h>
#include <skalibs/sgetopt.h>
#include <skalibs/types.h>
#include <skalibs/bitarray.h>
#include <skalibs/tai.h>
#include <skalibs/strerr2.h>
#include "s6-svlisten.h"

#define USAGE "s6-svwait [ -U | -u | -d | -D ] [ -a | -o ] [ -t timeout ] servicedir..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  tain_t deadline, tto ;
  int or = 0 ;
  int wantup = 1, wantready = 0 ;
  PROG = "s6-svwait" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    unsigned int t = 0 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "UudDaot:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'U' : wantup = 1 ; wantready = 1 ; break ;
        case 'u' : wantup = 1 ; wantready = 0 ; break ;
        case 'd' : wantup = 0 ; wantready = 0 ; break ;
        case 'D' : wantup = 0 ; wantready = 1 ; break ;
        case 'a' : or = 0 ; break ;
        case 'o' : or = 1 ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&tto, t) ; else tto = tain_infinite_relative ;
  }
  if (!argc) dieusage() ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&deadline, &tto) ;

  {
    s6_svlisten_t foo = S6_SVLISTEN_ZERO ;
    int e ;
    uint16_t ids[argc] ;
    unsigned char upstate[bitarray_div8(argc)] ;
    unsigned char readystate[bitarray_div8(argc)] ;
    s6_svlisten_init(argc, argv, &foo, ids, upstate, readystate, &deadline) ;
    e = s6_svlisten_loop(&foo, wantup, wantready, or, &deadline, -1, 0) ;
    if (e < 0) strerr_dief1x(102, "supervisor died") ;
    else if (e > 0) strerr_dief1x(e, "some services reported permanent failure") ; 
  }
  return 0 ;
}
