/* ISC license. */

#include <sys/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/types.h>
#include <skalibs/strerr.h>
#include <s6/ftrigw.h>

#define USAGE "s6-mkfifodir [ -f ] [ -g gid ] fifodir"

int main (int argc, char const *const *argv)
{
  subgetopt l = SUBGETOPT_ZERO ;
  gid_t gid = -1 ;
  int force = 0 ;
  PROG = "s6-mkfifodir" ;
  for (;;)
  {
    int opt = subgetopt_r(argc, argv, "fg:", &l) ;
    if (opt == -1) break ;
    switch (opt)
    {
      case 'f' : force = 1 ; break ;
      case 'g' : if (!gid0_scan(l.arg, &gid)) strerr_dieusage(100, USAGE) ; break ;
      default : strerr_dieusage(100, USAGE) ;
    }
  }
  argc -= l.ind ; argv += l.ind ;
  if (argc < 1) strerr_dieusage(100, USAGE) ;

  if (!ftrigw_fifodir_make(*argv, gid, force))
    strerr_diefu2sys(111, "create fifodir at ", *argv) ;
  return 0 ;
}
