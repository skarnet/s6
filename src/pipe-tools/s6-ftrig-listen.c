/* ISC license. */

#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <skalibs/posixplz.h>
#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/strerr.h>
#include <skalibs/gol.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/cspawn.h>

#include <s6/compat.h>
#include <s6/ftrigr.h>

#define USAGE "s6-ftrig-listen [ -a | -o ] [ -t timeout ] fifodir1 regexp1 ... \"\" prog..."
#define dieusage() strerr_dieusage(100, USAGE)

enum golb_e
{
  GOLB_OR = 0x01,
} ;

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

int main (int argc, char const **argv)
{
  static gol_bool const rgolb[] =
  {
    { .so = 'a', .lo = "and", .clear = GOLB_OR, .set = 0 },
    { .so = 'o', .lo = "or", .clear = 0, .set = GOLB_OR },
  } ;
  static gol_arg const rgola[GOLA_N] =
  {
    { .so = 't', .lo = "timeout", .i = GOLA_TIMEOUT },
  } ;
  char const *wgola[GOLA_N] = { 0 } ;
  uint64_t wgolb = 0 ;
  iopause_fd x[2] = { { .events = IOPAUSE_READ }, { .events = IOPAUSE_READ } } ;
  tain deadline ;
  tain tto = TAIN_INFINITE_RELATIVE ;
  ftrigr a = FTRIGR_ZERO ;
  int argc1 ;
  unsigned int golc ;

  PROG = "s6-ftrig-listen" ;
  golc = GOL_main(argc, argv, rgolb, rgola, &wgolb, wgola) ;
  argc -= golc ; argv += golc ;
  if (argc < 4) dieusage() ;
  if (wgola[GOLA_TIMEOUT])
  {
    unsigned int t = 0 ;
    if (!uint0_scan(wgola[GOLA_TIMEOUT], &t))
      strerr_dief1x(100, "timeout must be an unsigned integer") ;
    if (t) tain_from_millisecs(&tto, t) ;
  }

  argc1 = s6_el_semicolon(argv) ;
  if (!argc1 || (argc1 & 1) || (argc == argc1 + 1)) dieusage() ;
  if (argc1 >= argc) strerr_dief1x(100, "unterminated fifodir+regex block") ;
  if (!tain_now_set_stopwatch_g()) strerr_diefu1sys(111, "tain_now") ;
  tain_add_g(&deadline, &tto) ;
  x[0].fd = selfpipe_init() ;
  if (x[0].fd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
  if (!selfpipe_trap(SIGCHLD)) strerr_diefu1sys(111, "selfpipe_trap") ;
  if (!sig_altignore(SIGPIPE)) strerr_diefu1sys(111, "ignore SIGPIPE") ;

  if (!ftrigr_startf_g(&a, &deadline)) strerr_diefu1sys(111, "ftrigr_startf") ;
  x[1].fd = ftrigr_fd(&a) ;

  {
    pid_t pid = 0 ;
    unsigned int n = argc1 >> 1 ;
    uint32_t ids[n] ;
    for (unsigned int i = 0 ; i < n ; i++)
      if (!ftrigr_subscribe_g(&a, ids + i, 0, 0, argv[i<<1], argv[(i<<1)+1], &deadline))
        strerr_diefu4sys(111, "subscribe to ", argv[i<<1], " with regexp ", argv[(i<<1)+1]) ;

    pid = cspawn(argv[argc1 + 1], argv + argc1 + 1, (char const *const *)environ, CSPAWN_FLAGS_SELFPIPE_FINISH, 0, 0) ;
    if (!pid) strerr_diefu2sys(111, "spawn ", argv[argc1 + 1]) ;

    for (;;)
    {
      int r ;
      unsigned int i = 0 ;
      while (i < n)
      {
        struct iovec v ;
        r = ftrigr_peek(&a, ids[i], &v) ;
        if (r == -1) strerr_diefu1sys(111, "ftrigr_check") ;
        else if (!r) i++ ;
        else if (wgolb & GOLB_OR) n = 0 ;
        else ids[i] = ids[--n] ;
      }
      if (!n) break ;
      r = iopause_g(x, 2, &deadline) ;
      if (r == -1) strerr_diefu1sys(111, "iopause") ;
      else if (!r)
      {
        errno = ETIMEDOUT ;
        strerr_diefu1sys(1, "get expected event") ;
      }
      if (x[0].revents & IOPAUSE_READ) handle_signals() ;
      if (x[1].revents & IOPAUSE_READ)
        if (ftrigr_update(&a) < 0) strerr_diefu1sys(111, "ftrigr_update") ;
    }
  }
  _exit(0) ;
}
