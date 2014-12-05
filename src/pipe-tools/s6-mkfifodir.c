/* ISC license. */

#include <skalibs/sgetopt.h>
#include <skalibs/uint.h>
#include <skalibs/strerr2.h>
#include <s6/ftrigw.h>

#define USAGE "s6-mkfifodir [ -f ] [ -g gid ] fifodir"

int main (int argc, char const *const *argv)
{
  subgetopt_t l = SUBGETOPT_ZERO ;
  int gid = -1 ;
  int force = 0 ;
  PROG = "s6-mkfifodir" ;
  for (;;)
  {
    register int opt = subgetopt_r(argc, argv, "fg:", &l) ;
    if (opt == -1) break ;
    switch (opt)
    {
      case 'f' : force = 1 ; break ;
      case 'g' :
      {
        unsigned int g ;
        if (!uint0_scan(l.arg, &g)) strerr_dieusage(100, USAGE) ;
        gid = (int)g ;
        break ;
      }
      default : strerr_dieusage(100, USAGE) ;
    }
  }
  argc -= l.ind ; argv += l.ind ;
  if (argc < 1) strerr_dieusage(100, USAGE) ;

  if (!ftrigw_fifodir_make(*argv, gid, force))
    strerr_diefu2sys(111, "create fifodir at ", *argv) ;
  return 0 ;
}
