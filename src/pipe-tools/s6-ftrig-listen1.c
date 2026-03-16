/* ISC license. */

#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <skalibs/posixplz.h>
#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/gol.h>
#include <skalibs/strerr.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/cspawn.h>

#include <s6/ftrigr.h>

#define USAGE "s6-ftrig-listen1 [ -t timeout ] fifodir regexp prog..."

enum gola_e
{
  GOLA_TIMEOUT,
  GOLA_N
} ;

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

int main (int argc, char const *const *argv)
{
  static gol_arg const rgola[GOLA_N] =
  {
    { .so = 't', .lo = "timeout", .i = GOLA_TIMEOUT },
  } ;
  char const *wgola[GOLA_N] = { 0 } ;
  iopause_fd x[2] = { { .events = IOPAUSE_READ }, { .events = IOPAUSE_READ } } ;
  tain deadline ;
  tain tto = TAIN_INFINITE_RELATIVE ;
  ftrigr a = FTRIGR_ZERO ;
  pid_t pid ;
  uint32_t id ;
  struct iovec v[2] = { [1] = { .iov_base = "\n", .iov_len = 1 } } ;
  unsigned int golc ;

  PROG = "s6-ftrig-listen1" ;
  golc = gol_main(argc, argv, 0, 0, rgola, GOLA_N, 0, wgola) ;
  argc -= golc ; argv += golc ;
  if (argc < 3) strerr_dieusage(100, USAGE) ;
  if (wgola[GOLA_TIMEOUT])
  {
    unsigned int t = 0 ;
    if (!uint0_scan(wgola[GOLA_TIMEOUT], &t))
      strerr_dief1x(100, "timeout must be an unsigned integer") ;
    if (t) tain_from_millisecs(&tto, t) ;
  }

  if (!tain_now_set_stopwatch_g()) strerr_diefu1sys(111, "tain_now") ;
  tain_add_g(&deadline, &tto) ;

  if (!sig_altignore(SIGPIPE)) strerr_diefu1sys(111, "sig_ignore") ;
  if (!ftrigr_startf_g(&a, &deadline)) strerr_diefu1sys(111, "ftrigr_startf") ;
  if (!ftrigr_subscribe_g(&a, &id, 0, 0, argv[0], argv[1], &deadline))
    strerr_diefu4sys(111, "subscribe to ", argv[0], " with regexp ", argv[1]) ;

  x[0].fd = selfpipe_init() ;
  if (x[0].fd == -1) strerr_diefu1sys(111, "selfpipe_init") ;
  if (!selfpipe_trap(SIGCHLD)) strerr_diefu1sys(111, "selfpipe_trap") ;
  x[1].fd = ftrigr_fd(&a) ;

  pid = cspawn(argv[2], argv+2, (char const *const *)environ, CSPAWN_FLAGS_SELFPIPE_FINISH, 0, 0) ;
  if (!pid) strerr_diefu2sys(111, "spawn ", argv[2]) ;

  for (;;)
  {
    int r = ftrigr_peek(&a, id, &v[0]) ;
    if (r == -1) strerr_diefu1sys(111, "ftrigr_peek") ;
    if (r) break ;
    r = iopause_g(x, 2, &deadline) ;
    if (r == -1) strerr_diefu1sys(111, "iopause") ;
    else if (!r)
    {
      errno = ETIMEDOUT ;
      strerr_diefu1sys(1, "get expected event") ;
    }
    if (x[0].revents & IOPAUSE_READ) handle_signals() ;
    if (x[1].revents & IOPAUSE_READ)
      if (ftrigr_update(&a) == -1) strerr_diefu1sys(111, "ftrigr_update") ;
  }

  if (allwritev(1, v, 2) < 2) strerr_diefu1sys(111, "write to stdout") ;
  _exit(0) ;
}
