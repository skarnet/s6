/* ISC license. */

#include <string.h>

#include <skalibs/bytestr.h>
#include <skalibs/types.h>
#include <skalibs/strerr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/exec.h>

#include <s6/config.h>

#define USAGE "s6-instance-control [ -wu | -wU | -wd | -wD | -wr | -wR ] [ -T timeout ] [ -abqhkti12pcyroduDUxO ] service instancename"
#define dieusage() strerr_dieusage(100, USAGE)

#define DATASIZE 63

int main (int argc, char const **argv)
{
  char const **fullargv = argv ;
  size_t namelen ;
  PROG = "s6-instance-control" ;

  {
    subgetopt l = SUBGETOPT_ZERO ;
    unsigned int datalen = 1 ;
    unsigned int timeout = 0 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "abqhkti12pcyroduDUxOT:w:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'a' :
        case 'b' :
        case 'q' :
        case 'h' :
        case 'k' :
        case 't' :
        case 'i' :
        case '1' :
        case '2' :
        case 'p' :
        case 'c' :
        case 'y' :
        case 'r' :
        case 'o' :
        case 'd' :
        case 'u' :
        case 'D' :
        case 'U' :
        case 'x' :
        case 'O' : if (datalen++ >= DATASIZE) strerr_dief1x(100, "too many commands") ; break ;
        case 'T' : if (!uint0_scan(l.arg, &timeout)) dieusage() ; break ;
        case 'w' : if (!memchr("dDuUrR", l.arg[0], 6)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }

  if (argc < 2) dieusage() ;
  namelen = strlen(argv[1]) ;
  if (!argv[0][0]) strerr_dief1x(100, "invalid service name") ;
  if (!argv[1][0] || argv[1][0] == '.' || byte_in(argv[1], namelen, " \t\f\r\n", 5) < namelen)
    strerr_dief1x(100, "invalid instance name") ;

  {
    size_t svlen = strlen(argv[0]) ;
    char fn[svlen + 11 + namelen] ;
    memcpy(fn, argv[0], svlen) ;
    memcpy(fn + svlen, "/instance/", 10) ;
    memcpy(fn + svlen + 10, argv[1], namelen + 1) ;
    argv[0] = fn ;
    argv[1] = 0 ;
    fullargv[0] = S6_BINPREFIX "s6-svc" ;
    xexec(fullargv) ;
  }
}
