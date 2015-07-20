/* ISC license. */

#include <unistd.h>
#include <errno.h>
#include <skalibs/uint.h>
#include <skalibs/bytestr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <s6/config.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-svc [ -wu | -wU | -wd | -wD ] [ -T timeout ] [ -abqhkti12pcoduxOX ] servicedir"
#define dieusage() strerr_dieusage(100, USAGE)

#define DATASIZE 63

int main (int argc, char const *const *argv, char const *const *envp)
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
      register int opt = subgetopt_r(argc, argv, "abqhkti12pcoduxOXT:w:", &l) ;
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
        case 'o' :
        case 'd' :
        case 'u' :
        case 'x' :
        case 'O' :
        case 'X' :
        {
          if (datalen >= DATASIZE) strerr_dief1x(100, "too many commands") ;
          data[datalen++] = opt ;
          break ;
        }
        case 'T' : if (!uint0_scan(l.arg, &timeout)) dieusage() ; break ;
        case 'w' :
        {
          if (byte_chr("dDuU", 4, l.arg[0]) >= 4) dieusage() ;
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

  if (datalen <= 1) return 0 ;
  if (updown[1] == 'U')
  {
    unsigned int arglen = str_len(argv[0]) ;
    char fn[arglen + 17] ;
    byte_copy(fn, arglen, argv[0]) ;
    byte_copy(fn + arglen, 17, "/notification-fd") ;
    if (access(fn, F_OK) < 0)
    {
      if (errno != ENOENT) strerr_diefu2sys(111, "access ", fn) ;
      updown[1] = 'u' ;
      strerr_warnw2x(fn, " not present - converting -wU to -wu") ;
    }
  }

  if (updown[1])
  {
    char const *newargv[11] ;
    unsigned int m = 0 ;
    char fmt[UINT_FMT] ;
    newargv[m++] = S6_BINPREFIX "s6-svlisten1" ;
    newargv[m++] = updown ;
    if (timeout)
    {
      fmt[uint_fmt(fmt, timeout)] = 0 ;
      newargv[m++] = "-t" ;
      newargv[m++] = fmt ;
    }
    newargv[m++] = "--" ;
    newargv[m++] = argv[0] ;
    newargv[m++] = S6_BINPREFIX "s6-svc" ;
    newargv[m++] = data ;
    newargv[m++] = "--" ;
    newargv[m++] = argv[0] ;
    newargv[m++] = 0 ;
    pathexec_run(newargv[0], newargv, envp) ;
    strerr_dieexec(111, newargv[0]) ;
  }
  else
  {
    register int r = s6_svc_writectl(argv[0], S6_SUPERVISE_CTLDIR, data + 1, datalen - 1) ;
    if (r < 0) strerr_diefu2sys(111, "control ", argv[0]) ;
    else if (!r) strerr_diefu3x(100, "control ", argv[0], ": supervisor not listening") ;
  }
  return 0 ;
}
