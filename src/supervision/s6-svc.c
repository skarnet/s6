/* ISC license. */

#include <skalibs/uint.h>
#include <skalibs/bytestr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <s6/config.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-svc [ -D | -U ] [ -T timeout ] [ -abqhkti12pcoduxO ] servicedir"
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
      register int opt = subgetopt_r(argc, argv, "DUabqhkti12pcoduxOT:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'D' : updown[1] = 'd' ; break ;
        case 'U' : updown[1] = 'U' ; break ;
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
        {
          if (datalen >= DATASIZE) strerr_dief1x(100, "too many commands") ;
          data[datalen++] = opt ;
          break ;
        }
        case 'T' : if (!uint0_scan(l.arg, &timeout)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) dieusage() ;
  if (argc > 1) strerr_warn1x("ignoring extra arguments") ;
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
  else if (datalen > 1)
  {
    unsigned int arglen = str_len(argv[0]) ;
    char tmp[arglen + 9 + sizeof(S6_SUPERVISE_CTLDIR)] ;
    register int r ;
    byte_copy(tmp, arglen, argv[0]) ;
    tmp[arglen] = '/' ;
    byte_copy(tmp + arglen + 1, 8 + sizeof(S6_SUPERVISE_CTLDIR), S6_SUPERVISE_CTLDIR "/control") ;
    r = s6_svc_write(tmp, data + 1, datalen - 1) ;
    if (r < 0) strerr_diefu2sys(111, "control ", argv[0]) ;
    else if (!r) strerr_diefu3x(100, "control ", argv[0], ": supervisor not listening") ;
  }
  return 0 ;
}
