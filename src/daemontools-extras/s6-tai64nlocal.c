/* ISC license. */

#include <sys/types.h>
#include <errno.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/djbtime.h>
#include <skalibs/stralloc.h>
#include <skalibs/skamisc.h>

#define USAGE "s6-tai64nlocal [ -g ]"

int main (int argc, char const *const *argv)
{
  int islocal = 1 ;
  PROG = "s6-tai64nlocal" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "g", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'g' : islocal = 0 ; break ;
        default : strerr_dieusage(100, USAGE) ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }

  for (;;)
  {
    unsigned int p = 0 ;
    int r = skagetln(buffer_0f1, &satmp, '\n') ;
    if (r == -1)
      if (errno != EPIPE)
        strerr_diefu1sys(111, "read from stdin") ;
      else r = 1 ;
    else if (!r) break ;
    if (satmp.len > TIMESTAMP)
    {
      tain a ;
      p = timestamp_scan(satmp.s, &a) ;
      if (p)
      {
        localtmn local ;
        if (localtmn_from_tain(&local, &a, islocal))
        {
          char fmt[LOCALTMN_FMT+1] ;
          size_t len = localtmn_fmt(fmt, &local) ;
          if (buffer_put(buffer_1, fmt, len) < (ssize_t)len)
            strerr_diefu1sys(111, "write to stdout") ;
        }
        else p = 0 ;
      }
    }
    if (buffer_put(buffer_1, satmp.s + p, satmp.len - p) < (ssize_t)(satmp.len - p))
      strerr_diefu1sys(111, "write to stdout") ;
    satmp.len = 0 ;
  }
  return 0 ;
}
