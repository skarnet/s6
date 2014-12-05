/* ISC license. */

#include <skalibs/bytestr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/skamisc.h>
#include <s6/s6-supervise.h>

#define DATASIZE 256

int s6_svc_main (int argc, char const *const *argv, char const *optstring, char const *usage, char const *controldir)
{
  char data[DATASIZE] ;
  unsigned int datalen = 0 ;
  register int r ;
  for (;;)
  {
    register int opt = subgetopt(argc, argv, optstring) ;
    if (opt == -1) break ;
    if (opt == '?') strerr_dieusage(100, usage) ;
    if (datalen >= DATASIZE) strerr_dief1x(100, "too many commands") ;
    data[datalen++] = opt ;
  }
  argc -= subgetopt_here.ind ; argv += subgetopt_here.ind ;
  if (!argc) strerr_dieusage(100, usage) ;

  {
    unsigned int arglen = str_len(*argv) ;
    unsigned int cdirlen = str_len(controldir) ;
    char tmp[arglen + cdirlen + 10] ;
    byte_copy(tmp, arglen, *argv) ;
    tmp[arglen] = '/' ;
    byte_copy(tmp + arglen + 1, cdirlen, controldir) ;
    byte_copy(tmp + arglen + 1 + cdirlen, 9, "/control") ;
    r = s6_svc_write(tmp, data, datalen) ;
  }
  if (r < 0) strerr_diefu2sys(111, "control ", *argv) ;
  else if (!r) strerr_diefu3x(100, "control ", *argv, ": supervisor not listening") ;
  return 0 ;
}
