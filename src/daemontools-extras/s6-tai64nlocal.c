/* ISC license. */

#include <sys/types.h>
#include <errno.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/fmtscan.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <skalibs/djbtime.h>
#include <skalibs/stralloc.h>
#include <skalibs/skamisc.h>

int main (void)
{
  PROG = "s6-tai64nlocal" ;
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
      tain_t a ;
      p = timestamp_scan(satmp.s, &a) ;
      if (p)
      {
        char fmt[LOCALTMN_FMT+1] ;
        localtmn_t local ;
        size_t len ;
        localtmn_from_tain(&local, &a, 1) ;
        len = localtmn_fmt(fmt, &local) ;
        if (buffer_put(buffer_1, fmt, len) < 0)
          strerr_diefu1sys(111, "write to stdout") ;
      }
    }
    if (buffer_put(buffer_1, satmp.s + p, satmp.len - p) < 0)
      strerr_diefu1sys(111, "write to stdout") ;
    satmp.len = 0 ;
  }
  return 0 ;
}
