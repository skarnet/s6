/* ISC license. */

#include <sys/types.h>
#include <errno.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr.h>
#include <skalibs/tai.h>
#include <skalibs/stralloc.h>
#include <skalibs/skamisc.h>

int main (void)
{
  char stamp[TIMESTAMP+1] ;
  PROG = "s6-tai64n" ;
  stamp[TIMESTAMP] = ' ' ;
  for (;;)
  {
    int r = skagetln(buffer_0f1, &satmp, '\n') ;
    if (r < 0)
      if (errno != EPIPE)
        strerr_diefu1sys(111, "read from stdin") ;
      else
      {
        r = 1 ;
        if (!stralloc_catb(&satmp, "\n", 1))
          strerr_diefu1sys(111, "add newline") ;
      }
    else if (!r) break ;
    timestamp(stamp) ;
    if ((buffer_put(buffer_1, stamp, TIMESTAMP+1) < TIMESTAMP+1)
     || (buffer_put(buffer_1, satmp.s, satmp.len) < (ssize_t)satmp.len))
      strerr_diefu1sys(111, "write to stdout") ;
    satmp.len = 0 ;
  }
  return 0 ;
}
