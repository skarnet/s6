/* ISC license. */

#include <skalibs/uint.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <s6/s6-fdholder.h>

#define USAGE "s6-fdholder-retrievec [ -D ] [ -t timeout ] id prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  s6_fdholder_t a = S6_FDHOLDER_ZERO ;
  tain_t deadline ;
  int fd ;
  int dodelete = 0 ;
  PROG = "s6-fdholder-retrievec" ;
  {
    unsigned int t = 0 ;
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "Dt:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'D' : dodelete = 1 ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&deadline, t) ;
    else deadline = tain_infinite_relative ;
  }
  if (!argc) dieusage() ;

  s6_fdholder_init(&a, 6) ;
  tain_now_g() ;
  tain_add_g(&deadline, &deadline) ;
  fd = s6_fdholder_retrieve_maybe_delete_g(&a, argv[0], dodelete, &deadline) ;
  if (fd < 0) strerr_diefu2sys(1, "retrieve fd for id ", argv[0]) ;
  else if (!fd)
  {
    if (uncoe(0) < 0) strerr_diefu1sys(111, "uncoe stdin") ;
  }
  else if (fd_move(0, fd) < 0) strerr_diefu1sys(111, "move fd") ;
  pathexec_run(argv[1], argv+1, envp) ;
  strerr_dieexec(111, argv[1]) ;
}
