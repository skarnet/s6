/* ISC license. */

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <skalibs/sgetopt.h>
#include <skalibs/bytestr.h>
#include <skalibs/uint16.h>
#include <skalibs/uint.h>
#include <skalibs/tai.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <skalibs/iopause.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <s6/ftrigr.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-svlisten1 [ -U | -u | -d ] [ -t timeout ] servicedir prog..."
#define dieusage() strerr_dieusage(100, USAGE)

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
  ftrigr_t a = FTRIGR_ZERO ;
  tain_t deadline, tto ;
  int spfd ;
  int wantup = 1 ;
  char re[4] = "u|d" ;
  PROG = "s6-svlisten1" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    unsigned int t = 0 ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "uUdt:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'U' : wantup = 1 ; re[0] = 'U' ; break ;
        case 'u' : wantup = 1 ; re[0] = 'u' ; break ;
        case 'd' : wantup = 0 ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&tto, t) ; else tto = tain_infinite_relative ;
  }
  if (!argc) dieusage() ;

  spfd = selfpipe_init() ;
  if (spfd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
  if (selfpipe_trap(SIGCHLD) < 0) strerr_diefu1sys(111, "selfpipe_trap") ;
  if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "sig_ignore") ;

  tain_now_g() ;
  tain_add_g(&deadline, &tto) ;

  if (!ftrigr_startf_g(&a, &deadline)) strerr_diefu1sys(111, "ftrigr_startf") ;

  {
    iopause_fd x[2] = { { .fd = spfd, .events = IOPAUSE_READ }, { .fd = ftrigr_fd(&a), .events = IOPAUSE_READ } } ;
    unsigned int arglen = str_len(argv[0]) ;
    pid_t pid ;
    int isup ;
    s6_svstatus_t st = S6_SVSTATUS_ZERO ;
    uint16 id ;
    char s[arglen + 1 + sizeof(S6_SUPERVISE_EVENTDIR)] ;
    byte_copy(s, arglen, argv[0]) ;
    s[arglen] = '/' ;
    byte_copy(s + arglen + 1, sizeof(S6_SUPERVISE_EVENTDIR), S6_SUPERVISE_EVENTDIR) ;
    id = ftrigr_subscribe_g(&a, s, re, FTRIGR_REPEAT, &deadline) ;
    if (!id) strerr_diefu2sys(111, "subscribe to events for ", argv[0]) ;
    if (!s6_svstatus_read(argv[0], &st)) strerr_diefu1sys(111, "s6_svstatus_read") ;
    isup = !!st.pid ;
    if (re[0] == 'U' && isup)
    {
      byte_copy(s + arglen + 1, sizeof(S6_SUPERVISE_READY_FILENAME), S6_SUPERVISE_READY_FILENAME) ;
      if (access(s, F_OK) < 0)
      {
        if (errno == ENOENT) isup = 0 ;
        else strerr_warnwu2sys("check ", s) ;
      }
    }

    pid = child_spawn0(argv[1], argv + 1, envp) ;
    if (!pid) strerr_diefu2sys(111, "spawn ", argv[2]) ;

    for (;;)
    {
      register int r ;
      if (isup == wantup) break ;
      r = iopause_g(x, 2, &deadline) ;
      if (r < 0) strerr_diefu1sys(111, "iopause") ;
      else if (!r) strerr_dief1x(1, "timed out") ;

      if (x[0].revents & IOPAUSE_READ) handle_signals() ;
      if (x[1].revents & IOPAUSE_READ)
      {
        char what ;
        if (ftrigr_update(&a) < 0) strerr_diefu1sys(111, "ftrigr_update") ;
        r = ftrigr_check(&a, id, &what) ;
        if (r < 0) strerr_diefu1sys(111, "ftrigr_check") ;
        if (r) isup = what == re[0] ;
      }
    }
  }
  return 0 ;
}
