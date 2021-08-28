/* ISC license. */

#include <stdint.h>

#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>

#include <s6/s6-supervise.h>

#define USAGE "s6-svdir-unlink [ -d ] [ -x ] scandir servicename"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  uint32_t options = 0 ;
  PROG = "s6-svdir-link" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "dx", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'd' : options = 3 ; break ;
        case 'x' : options = 1 ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (argc < 2) dieusage() ;

  s6_supervise_unlink(argv[0], argv[1], options) ;
  return 0 ;
}
