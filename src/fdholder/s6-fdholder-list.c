/* ISC license. */

#include <string.h>
#include <skalibs/types.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/stralloc.h>
#include <skalibs/skamisc.h>
#include <s6/s6-fdholder.h>

#define USAGE "s6-fdholder-list [ -t timeout ] socket"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  s6_fdholder_t a = S6_FDHOLDER_ZERO ;
  stralloc sa = STRALLOC_ZERO, sb = STRALLOC_ZERO ;
  size_t pos = 0 ;
  int n ;
  tain_t deadline ;
  PROG = "s6-fdholder-list" ;
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
  tain_now_set_stopwatch_g() ;
  tain_add_g(&deadline, &deadline) ;
  if (!s6_fdholder_start_g(&a, argv[0], &deadline))
    strerr_diefu2sys(111, "connect to a fd-holder daemon at ", argv[0]) ;
  n = s6_fdholder_list_g(&a, &sa, &deadline) ;
  if (n < 0) strerr_diefu1sys(1, "get fd list") ;
  while (n--)
  {
    size_t len = strlen(sa.s + pos) ;
    sb.len = 0 ;
    if (!string_quote_nodelim_mustquote(&sb, sa.s + pos, len, 0, 0))
      strerr_diefu1sys(111, "quote string") ;
    if (buffer_put(buffer_1, sb.s, sb.len) < sb.len || buffer_put(buffer_1, "\n", 1) < 1)
      strerr_diefu1sys(111, "buffer_put") ;
    pos += len+1 ;
  }
  stralloc_free(&sb) ;
  stralloc_free(&sa) ;
  if (!buffer_flush(buffer_1)) strerr_diefu1sys(111, "write to stdout") ;
  return 0 ;
}
