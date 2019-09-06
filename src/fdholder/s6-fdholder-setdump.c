/* ISC license. */

#include <string.h>
#include <stdlib.h>
#include <skalibs/types.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <s6/s6-fdholder.h>

#define USAGE "s6-fdholder-setdump [ -t timeout ] socket"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  s6_fdholder_t a = S6_FDHOLDER_ZERO ;
  tain_t deadline ;
  unsigned int dumplen ;
  char const *x ;
  PROG = "s6-fdholder-setdump" ;
  {
    unsigned int t = 0 ;
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "t:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&deadline, t) ;
    else deadline = tain_infinite_relative ;
  }
  if (!argc) dieusage() ;

  x = getenv("S6_FD#") ;
  if (!x) strerr_dienotset(100, "S6_FD#") ;
  if (!uint0_scan(x, &dumplen)) strerr_dieinvalid(100, "S6_FD#") ;
  tain_now_set_stopwatch_g() ;
  tain_add_g(&deadline, &deadline) ;
  if (!s6_fdholder_start_g(&a, argv[0], &deadline))
    strerr_diefu2sys(111, "connect to a fd-holder daemon at ", argv[0]) ;
  if (dumplen)
  {
    unsigned int i = 0 ;
    s6_fdholder_fd_t dump[dumplen] ;
    char s[11 + UINT_FMT] ;
    for (; i < dumplen ; i++)
    {
      size_t len ;
      unsigned int fd ;
      memcpy(s, "S6_FD_", 6) ;
      s[6 + uint_fmt(s+6, i)] = 0 ;
      x = getenv(s) ;
      if (!x) strerr_dienotset(100, s) ;
      if (!uint0_scan(x, &fd)) strerr_dieinvalid(100, s) ;
      dump[i].fd = fd ;
      memcpy(s, "S6_FDID_", 8) ;
      s[8 + uint_fmt(s+8, i)] = 0 ;
      x = getenv(s) ;
      if (!x) strerr_dienotset(100, s) ;
      len = strlen(x) ;
      if (!len || len > S6_FDHOLDER_ID_SIZE) strerr_dieinvalid(100, s) ;
      memcpy(dump[i].id, x, len+1) ;
      memcpy(s, "S6_FDLIMIT_", 11) ;
      s[11 + uint_fmt(s+11, i)] = 0 ;
      x = getenv(s) ;
      if (!x) tain_add_g(&dump[i].limit, &tain_infinite_relative) ;
      else if (!timestamp_scan(x, &dump[i].limit)) strerr_dieinvalid(100, s) ;
    }
    if (!s6_fdholder_setdump_g(&a, dump, dumplen, &deadline))
      strerr_diefu1sys(1, "set dump") ;
  }
  return 0 ;
}
