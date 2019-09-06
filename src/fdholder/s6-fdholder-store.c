/* ISC license. */

#include <skalibs/types.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <s6/s6-fdholder.h>

#define USAGE "s6-fdholder-store [ -d fd ] [ -t timeout ] [ -T fdtimeout ] socket id"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  s6_fdholder_t a = S6_FDHOLDER_ZERO ;
  tain_t deadline, limit ;
  unsigned int fd = 0 ;
  PROG = "s6-fdholder-store" ;
  {
    unsigned int t = 0, T = 0 ;
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "d:t:T:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'd' : if (!uint0_scan(l.arg, &fd)) dieusage() ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        case 'T' : if (!uint0_scan(l.arg, &T)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&deadline, t) ;
    else deadline = tain_infinite_relative ;
    if (T) tain_from_millisecs(&limit, T) ;
    else limit = tain_infinite_relative ;
  }
  if (argc < 2) dieusage() ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&deadline, &deadline) ;
  tain_add_g(&limit, &limit) ;
  if (!s6_fdholder_start_g(&a, argv[0], &deadline))
    strerr_diefu2sys(111, "connect to a fd-holder daemon at ", argv[0]) ;
  if (!s6_fdholder_store_g(&a, fd, argv[1], &limit, &deadline))
    strerr_diefu1sys(1, "store fd") ;
  return 0 ;
}
