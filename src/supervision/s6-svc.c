/* ISC license. */

#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/exec.h>

#include <s6/config.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-svc [ -wu | -wU | -wd | -wD | -wr | -wR ] [ -T timeout ] [ -abqhkti12pcyroduxOX ] servicedir"
#define dieusage() strerr_dieusage(100, USAGE)

#define DATASIZE 63

int main (int argc, char const *const *argv)
{
  char data[DATASIZE+1] = "-" ;
  unsigned int datalen = 1 ;
  unsigned int timeout = 0 ;
  char updown[3] = "-\0" ;
  PROG = "s6-svc" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "abqhkti12pcyroduxOT:w:", &l) ;
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
        case 'x' :
        case 'O' :
        {
          if (datalen >= DATASIZE) strerr_dief1x(100, "too many commands") ;
          data[datalen++] = opt ;
          break ;
        }
        case 'T' : if (!uint0_scan(l.arg, &timeout)) dieusage() ; break ;
        case 'w' :
        {
          if (!memchr("dDuUrR", l.arg[0], 6)) dieusage() ;
          updown[1] = l.arg[0] ;
          break ;
        }
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) dieusage() ;
  if (argc > 1) strerr_warn1x("ignoring extra arguments") ;

  if (updown[1] == 'U' || updown[1] == 'R')
  {
    size_t arglen = strlen(argv[0]) ;
    char fn[arglen + 17] ;
    memcpy(fn, argv[0], arglen) ;
    memcpy(fn + arglen, "/notification-fd", 17) ;
    if (access(fn, F_OK) < 0)
    {
      if (errno != ENOENT) strerr_diefu2sys(111, "access ", fn) ;
      updown[1] = updown[1] == 'U' ? 'u' : 'r' ;
      strerr_warnw2x(fn, " not present - ignoring request for readiness notification") ;
    }
  }

  if (updown[1])
  {
    char const *newargv[11] ;
    unsigned int m = 0 ;
    char fmt[UINT_FMT] ;
    newargv[m++] = datalen > 1 ? S6_BINPREFIX "s6-svlisten1" : S6_BINPREFIX "s6-svwait" ;
    newargv[m++] = updown ;
    if (timeout)
    {
      fmt[uint_fmt(fmt, timeout)] = 0 ;
      newargv[m++] = "-t" ;
      newargv[m++] = fmt ;
    }
    newargv[m++] = "--" ;
    newargv[m++] = argv[0] ;
    if (datalen > 1)
    {
      newargv[m++] = S6_BINPREFIX "s6-svc" ;
      newargv[m++] = data ;
      newargv[m++] = "--" ;
      newargv[m++] = argv[0] ;
    }
    newargv[m++] = 0 ;
    xexec(newargv) ;
  }
  else switch (s6_svc_writectl(argv[0], S6_SUPERVISE_CTLDIR, data + 1, datalen - 1))
  {
    case -1 : strerr_diefu2sys(111, "control ", argv[0]) ;
    case -2 : strerr_dief3sys(100, "something is wrong with the ", argv[0], "/" S6_SUPERVISE_CTLDIR " directory") ;
    case 0 : strerr_diefu3x(100, "control ", argv[0], ": supervisor not listening") ;
  }
  return 0 ;
}
