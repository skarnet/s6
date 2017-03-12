/* ISC license. */

#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-svscanctl [ -phratszbnNiq0678 ] svscandir"
#define dieusage() strerr_dieusage(100, USAGE)

#define DATASIZE 64

int main (int argc, char const *const *argv)
{
  char data[DATASIZE] ;
  unsigned int datalen = 0 ;
  int r ;
  PROG = "s6-svscanctl" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "phratszbnNiq0678", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'p' :
        case 'h' :
        case 'r' :
        case 'a' :
        case 't' :
        case 's' :
        case 'z' :
        case 'b' :
        case 'n' :
        case 'N' :
        case 'i' :
        case 'q' :
        case '0' :
        case '6' :
        case '7' :
        case '8' :
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

  r = s6_svc_writectl(argv[0], S6_SVSCAN_CTLDIR, data, datalen) ;
  if (r < 0) strerr_diefu2sys(111, "control ", argv[0]) ;
  else if (!r) strerr_diefu3x(100, "control ", argv[0], ": supervisor not listening") ;
  return 0 ;
}
