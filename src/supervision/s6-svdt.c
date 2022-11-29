/* ISC license. */

#include <sys/types.h>
#include <skalibs/types.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/sig.h>
#include <s6/supervise.h>

#define USAGE "s6-svdt [ -S | -s ] [ -n maxentries ] servicedir"
#define dieusage() strerr_dieusage(100, USAGE)
#define die1() strerr_diefu1sys(111, "write to stdout")

int main (int argc, char const *const *argv)
{
  unsigned int n = S6_MAX_DEATH_TALLY ;
  int num = 0 ;
  PROG = "s6-svdt" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "Ssn:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'S' : num = 0 ; break ;
        case 's' : num = 1 ; break ;
        case 'n' : if (!uint0_scan(l.arg, &n)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) dieusage() ;
  if (n > S6_MAX_DEATH_TALLY) n = S6_MAX_DEATH_TALLY ;
  if (!n) return 0 ;
  {
    s6_dtally_t tab[n] ;
    ssize_t r = s6_dtally_read(argv[0], tab, n) ;
    if (r < 0) strerr_diefu2sys(111, "read death tally for service ", argv[0]) ;
    for (size_t i = 0 ; i < r ; i++)
    {
      char fmt[TIMESTAMP + 1] ;
      timestamp_fmt(fmt, &tab[i].stamp) ;
      fmt[TIMESTAMP] = ' ' ;
      if (buffer_put(buffer_1, fmt, TIMESTAMP + 1) < 0) die1() ;
      if (tab[i].sig)
      {
        if (buffer_puts(buffer_1, "signal ") < 0) die1() ;
        if (num)
        {
          char fmt[3] ;
          if (buffer_put(buffer_1, fmt, uint_fmt(fmt, tab[i].sig)) < 0) die1() ;
        }
        else
        {
          if (buffer_puts(buffer_1, "SIG") < 0) die1() ;
          if (buffer_puts(buffer_1, sig_name(tab[i].sig)) < 0) die1() ;
        }
      }
      else
      {
        char fmt[3] ;
        if (buffer_puts(buffer_1, "exitcode ") < 0) die1() ;
        if (buffer_put(buffer_1, fmt, uint_fmt(fmt, tab[i].exitcode)) < 0) die1() ;
      }
      if (buffer_put(buffer_1, "\n", 1) < 0) die1() ;
    }
  }
  if (!buffer_flush(buffer_1)) die1() ;
  return 0 ;
}
