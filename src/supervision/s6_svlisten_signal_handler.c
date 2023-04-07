/* ISC license. */

#include <signal.h>

#include <skalibs/strerr.h>
#include <skalibs/sig.h>
#include <skalibs/djbunix.h>
#include <skalibs/selfpipe.h>

#include "s6-svlisten.h"

void s6_svlisten_selfpipe_init (void)
{
  if (selfpipe_init() == -1) strerr_diefu1sys(111, "selfpipe_init") ;
  if (!selfpipe_trap(SIGCHLD)) strerr_diefu1sys(111, "selfpipe_trap") ;
  if (!sig_altignore(SIGPIPE)) strerr_diefu1sys(111, "ignore SIGPIPE") ;
}

void s6_svlisten_signal_handler (void)
{
  for (;;) switch (selfpipe_read())
  {
    case -1 : strerr_diefu1sys(111, "selfpipe_read") ;
    case 0 : return ;
    case SIGCHLD : wait_reap() ; break ;
    default : strerr_dief1x(101, "unexpected data in selfpipe") ;
  }
}
