/* ISC license. */

#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <skalibs/sgetopt.h>
#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <s6/ftrigr.h>

#define USAGE "s6-ftrig-listen1 [ -t timeout ] fifodir regexp prog..."

static void handle_signals (void)
{
  for (;;) switch (selfpipe_read())
  {
    case -1 : strerr_diefu1sys(111, "selfpipe_read") ;
    case 0 : return ;
    case SIGCHLD : wait_reap() ; break ;
    default : strerr_dief1x(101, "unexpected data in selfpipe") ;
  }
}

int main (int argc, char const *const *argv, char const *const *envp)
{
  iopause_fd x[2] = { { -1, IOPAUSE_READ, 0 }, { -1, IOPAUSE_READ, 0 } } ;
  tain_t deadline, tto ;
  ftrigr_t a = FTRIGR_ZERO ;
  pid_t pid ;
  uint16_t id ;
  char pack[2] = " \n" ;
  PROG = "s6-ftrig-listen1" ;
  {
    unsigned int t = 0 ;
    for (;;)
    {
      int opt = subgetopt(argc, argv, "t:") ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 't' : if (uint0_scan(subgetopt_here.arg, &t)) break ;
        default : strerr_dieusage(100, USAGE) ;
      }
    }
    if (t) tain_from_millisecs(&tto, t) ;
    else tto = tain_infinite_relative ;
    argc -= subgetopt_here.ind ; argv += subgetopt_here.ind ;
  }
  if (argc < 3) strerr_dieusage(100, USAGE) ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&deadline, &tto) ;

  if (!ftrigr_startf_g(&a, &deadline)) strerr_diefu1sys(111, "ftrigr_startf") ;
  id = ftrigr_subscribe_g(&a, argv[0], argv[1], 0, &deadline) ;
  if (!id) strerr_diefu4sys(111, "subscribe to ", argv[0], " with regexp ", argv[1]) ;

  x[0].fd = selfpipe_init() ;
  if (x[0].fd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
  if (selfpipe_trap(SIGCHLD) < 0) strerr_diefu1sys(111, "selfpipe_trap") ;
  if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "sig_ignore") ;
  x[1].fd = ftrigr_fd(&a) ;

  pid = child_spawn0(argv[2], argv+2, envp) ;
  if (!pid) strerr_diefu2sys(111, "spawn ", argv[2]) ;

  for (;;)
  {
    int r = ftrigr_check(&a, id, &pack[0]) ;
    if (r < 0) strerr_diefu1sys(111, "ftrigr_check") ;
    if (r) break ;
    r = iopause_g(x, 2, &deadline) ;
    if (r < 0) strerr_diefu1sys(111, "iopause") ;
    else if (!r)
    {
      errno = ETIMEDOUT ;
      strerr_diefu1sys(1, "get expected event") ;
    }
    if (x[0].revents & IOPAUSE_READ) handle_signals() ;
    if (x[1].revents & IOPAUSE_READ)
    {
      if (ftrigr_update(&a) < 0) strerr_diefu1sys(111, "ftrigr_update") ;
    }
  }

  if (allwrite(1, pack, 2) < 2) strerr_diefu1sys(111, "write to stdout") ;
  return 0 ;
}
