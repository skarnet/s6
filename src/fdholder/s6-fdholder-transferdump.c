/* ISC license. */

#include <skalibs/types.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/genalloc.h>
#include <s6/s6-fdholder.h>

#define USAGE "s6-fdholder-transferdump [ -t timeoutfrom:timeoutto ] socketfrom socketto"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  s6_fdholder_t a = S6_FDHOLDER_ZERO ;
  genalloc dump = GENALLOC_ZERO ; /* array of s6_fdholder_fd_t */
  tain_t deadline, totto ;
  PROG = "s6-fdholder-transferdump" ;
  {
    unsigned int timeoutfrom = 0, timeoutto = 0 ;
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "t:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 't' :
        {
          size_t pos = uint_scan(l.arg, &timeoutfrom) ;
          if (!pos)
          {
            if (l.arg[pos] != ':') dieusage() ;
            timeoutfrom = 0 ;
          }
          if (!l.arg[pos]) timeoutto = 0 ;
          else
          {
            if (l.arg[pos++] != ':') dieusage() ;
            if (!l.arg[pos]) timeoutto = 0 ;
            else if (!uint0_scan(l.arg + pos, &timeoutto)) dieusage() ;
          }
          break ;
        }
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (timeoutfrom) tain_from_millisecs(&deadline, timeoutfrom) ;
    else deadline = tain_infinite_relative ;
    if (timeoutto) tain_from_millisecs(&totto, timeoutto) ;
    else totto = tain_infinite_relative ;
  }
  if (argc < 2) dieusage() ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&deadline, &deadline) ;
  if (!s6_fdholder_start_g(&a, argv[0], &deadline))
    strerr_diefu2sys(111, "connect to a source fd-holder daemon at ", argv[0]) ;
  if (!s6_fdholder_getdump_g(&a, &dump, &deadline))
    strerr_diefu1sys(1, "get dump") ;
  s6_fdholder_end(&a) ;
  tain_add_g(&deadline, &totto) ;
  if (!s6_fdholder_start_g(&a, argv[1], &deadline))
    strerr_diefu2sys(111, "connect to a destination fd-holder daemon at ", argv[1]) ;
  if (!s6_fdholder_setdump_g(&a, genalloc_s(s6_fdholder_fd_t, &dump), genalloc_len(s6_fdholder_fd_t, &dump), &deadline))
    strerr_diefu1sys(1, "set dump") ;
  return 0 ;
}
