/* ISC license. */

#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <s6/s6-fdholder.h>

#define USAGE "s6-fdholder-delete [ -t timeout ] socket id"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  s6_fdholder_t a = S6_FDHOLDER_ZERO ;
  tain_t deadline ;
  PROG = "s6-fdholder-delete" ;
  {
    unsigned int t = 0 ;
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "t:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&deadline, t) ;
    else deadline = tain_infinite_relative ;
  }
  if (argc < 2) dieusage() ;
  tain_now_set_stopwatch_g() ;
  tain_add_g(&deadline, &deadline) ;
  if (!s6_fdholder_start_g(&a, argv[0], &deadline))
    strerr_diefu2sys(111, "connect to a fd-holder daemon at ", argv[0]) ;
  if (!s6_fdholder_delete_g(&a, argv[1], &deadline))
    strerr_diefu2sys(1, "delete fd for id ", argv[0]) ;
  return 0 ;
}
