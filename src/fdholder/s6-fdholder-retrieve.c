/* ISC license. */

#include <skalibs/types.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <s6/s6-fdholder.h>

#define USAGE "s6-fdholder-retrieve [ -D ] [ -t timeout ] socket id prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  s6_fdholder_t a = S6_FDHOLDER_ZERO ;
  tain_t deadline ;
  int fd ;
  int dodelete = 0 ;
  PROG = "s6-fdholder-retrieve" ;
  {
    unsigned int t = 0 ;
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "Dt:", &l) ;
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
  if (argc < 3) dieusage() ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&deadline, &deadline) ;
  if (!s6_fdholder_start_g(&a, argv[0], &deadline))
    strerr_diefu2sys(111, "connect to a fd-holder daemon at ", argv[0]) ;
  fd = s6_fdholder_retrieve_maybe_delete_g(&a, argv[1], dodelete, &deadline) ;
  if (fd < 0) strerr_diefu2sys(1, "retrieve fd for id ", argv[1]) ;
  s6_fdholder_end(&a) ;
  if (!fd)
  {
    if (uncoe(0) < 0) strerr_diefu1sys(111, "uncoe stdin") ;
  }
  else if (fd_move(0, fd) < 0) strerr_diefu1sys(111, "move fd") ;
  xpathexec_run(argv[2], argv+2, envp) ;
}
