/* ISC license. */

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <skalibs/bytestr.h>
#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/strerr.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <s6/supervise.h>

#define USAGE "s6-instance-delete [ -X ] [ -t timeout ] service instancename"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  tain tto = TAIN_INFINITE_RELATIVE ;
  uint32_t options = 1 ;
  PROG = "s6-instance-delete" ;
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
  if (!argv[0][0]) strerr_dief1x(100, "invalid service path") ;
  if (!argv[1][0] || argv[1][0] == '.' || byte_in(argv[1], strlen(argv[1]), " \t\f\r\n", 5) < strlen(argv[1]))
    strerr_dief1x(100, "invalid instance name") ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&tto, &tto) ;

  {
    size_t svlen = strlen(argv[0]) ;
    char sc[svlen + 10] ;
    memcpy(sc, argv[0], svlen) ;
    memcpy(sc + svlen, "/instance", 10) ;
    if (s6_supervise_unlink_names_g(sc, argv + 1, 1, options, &tto) == -1)
      strerr_diefu4sys(111, "prepare deletion of instance ", argv[1], " of service ", argv[0]) ;
  }

  return 0 ;
}
