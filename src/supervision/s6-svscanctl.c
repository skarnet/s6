/* ISC license. */

#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-svscanctl [ -zabhitqnN ] svscandir"
#define dieusage() strerr_dieusage(100, USAGE)

#define DATASIZE 64

int main (int argc, char const *const *argv)
{
  char data[DATASIZE] ;
  unsigned int datalen = 0 ;
  PROG = "s6-svscanctl" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "phratszbnNiq0678", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'z' :
        case 'a' :
        case 'b' :
        case 'h' :
        case 'i' :
        case 't' :
        case 'q' :
        case 'n' :
        case 'N' :
        {
          if (datalen >= DATASIZE) strerr_dief1x(100, "too many commands") ;
          data[datalen++] = opt ;
          break ;
        }
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) dieusage() ;

  switch (s6_svc_writectl(argv[0], S6_SVSCAN_CTLDIR, data, datalen))
  {
    case -1 : strerr_diefu2sys(111, "control ", argv[0]) ;
    case -2 : strerr_dief3sys(100, "something is wrong with the ", argv[0], "/" S6_SVSCAN_CTLDIR " directory. errno reported") ;
    case 0 : strerr_diefu3x(100, "control ", argv[0], ": supervisor not listening") ;
  }
  return 0 ;
}
