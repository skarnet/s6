/* ISC license. */

#include <stdint.h>

#include <skalibs/posixplz.h>
#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr.h>
#include <skalibs/tai.h>

#include <s6/supervise.h>

#define USAGE "s6-svunlink [ -X ] [ -t timeout ] scandir servicename"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  tain tto = TAIN_INFINITE_RELATIVE ;
  uint32_t options = 1 ;
  PROG = "s6-svunlink" ;
  {
    unsigned int t = 0 ;
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "Xt:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'X' : options &= ~1U ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&tto, t) ;
  }
  if (argc < 2) dieusage() ;

  if (!argv[1][0] || argv[1][0] == '.' || argv[1][0] == '/')
    strerr_dief1x(100, "invalid service name") ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&tto, &tto) ;

  if (s6_supervise_unlink_names_g(argv[0], argv + 1, 1, options, &tto) == -1)
    strerr_diefu4sys(111, "prepare unlinking of service ", argv[1], " in scandir ", argv[0]) ;
  return 0 ;
}
