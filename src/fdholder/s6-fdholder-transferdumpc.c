/* ISC license. */

#include <skalibs/uint.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/genalloc.h>
#include <s6/s6-fdholder.h>

#define USAGE "s6-fdholder-transferdumpc [ -t timeoutfrom ] [ -T timeoutto ]"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  s6_fdholder_t a = S6_FDHOLDER_ZERO ;
  genalloc dump = GENALLOC_ZERO ;
  tain_t deadline, totto ;
  PROG = "s6-fdholder-transferdumpc" ;
  {
    unsigned int t = 0, T = 0 ;
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "t:T:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        case 'T' : if (!uint0_scan(l.arg, &T)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&deadline, t) ;
    else deadline = tain_infinite_relative ;
    if (T) tain_from_millisecs(&totto, T) ;
    else totto = tain_infinite_relative ;
  }

  s6_fdholder_init(&a, 0) ;
  tain_now_g() ;
  tain_add_g(&deadline, &deadline) ;
  if (!s6_fdholder_getdump_g(&a, &dump, &deadline))
    strerr_diefu1sys(1, "get dump") ;
  s6_fdholder_free(&a) ;
  s6_fdholder_init(&a, 1) ;
  tain_add_g(&deadline, &totto) ;
  if (!s6_fdholder_setdump_g(&a, genalloc_s(s6_fdholder_fd_t, &dump), genalloc_len(s6_fdholder_fd_t, &dump), &deadline))
    strerr_diefu1sys(1, "set dump") ;
  return 0 ;
}
