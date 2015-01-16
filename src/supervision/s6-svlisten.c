/* ISC license. */

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <skalibs/sgetopt.h>
#include <skalibs/bytestr.h>
#include <skalibs/uint16.h>
#include <skalibs/uint.h>
#include <skalibs/bitarray.h>
#include <skalibs/tai.h>
#include <skalibs/strerr2.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <execline/execline.h>
#include <s6/ftrigr.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-svlisten [ -U | -u | -d ] [ -A | -a | -o ] [ -t timeout ] servicedir... \"\" prog..."
#define dieusage() strerr_dieusage(100, USAGE)

static inline int check (unsigned char const *ba, unsigned int n, int wantup, int or)
{
  return (bitarray_first(ba, n, or == wantup) < n) == or ;
}

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

int main (int argc, char const **argv, char const *const *envp)
{
  ftrigr_t a = FTRIGR_ZERO ;
  tain_t deadline, tto ;
  int spfd ;
  int argc1 ;
  int or = 0 ;
  int wantup = 1 ;
  char re[4] = "u|d" ;
  PROG = "s6-svlisten" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    unsigned int t = 0 ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "uUdAaot:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'U' : wantup = 1 ; re[0] = 'U' ; break ;
        case 'u' : wantup = 1 ; re[0] = 'u' ; break ;
        case 'd' : wantup = 0 ; break ;
        case 'a' : or = 0 ; break ;
        case 'o' : or = 1 ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&tto, t) ; else tto = tain_infinite_relative ;
  }
  if (argc < 3) dieusage() ;

  argc1 = el_semicolon(argv) ;
  if (!argc1 || argc == argc1 + 1) dieusage() ;
  if (argc1 >= argc) strerr_dief1x(100, "unterminated servicedir block") ;

  spfd = selfpipe_init() ;
  if (spfd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
  if (selfpipe_trap(SIGCHLD) < 0) strerr_diefu1sys(111, "selfpipe_trap") ;
  if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "sig_ignore") ;

  tain_now_g() ;
  tain_add_g(&deadline, &tto) ;

  if (!ftrigr_startf_g(&a, &deadline)) strerr_diefu1sys(111, "ftrigr_startf") ;

  {
    iopause_fd x[2] = { { .fd = spfd, .events = IOPAUSE_READ }, { .fd = ftrigr_fd(&a), .events = IOPAUSE_READ } } ;
    pid_t pid ;
    unsigned int i = 0 ;
    uint16 list[argc1] ;
    unsigned char states[bitarray_div8(argc1)] ;
    for (; i < (unsigned int)argc1 ; i++)
    {
      unsigned int len = str_len(argv[i]) ;
      char s[len + 1 + sizeof(S6_SUPERVISE_EVENTDIR)] ;
      byte_copy(s, len, argv[i]) ;
      s[len] = '/' ;
      byte_copy(s + len + 1, sizeof(S6_SUPERVISE_EVENTDIR), S6_SUPERVISE_EVENTDIR) ;
      list[i] = ftrigr_subscribe_g(&a, s, re, FTRIGR_REPEAT, &deadline) ;
      if (!list[i]) strerr_diefu2sys(111, "subscribe to events for ", argv[i]) ;
    }

    for (i = 0 ; i < (unsigned int)argc1 ; i++)
    {
      s6_svstatus_t st = S6_SVSTATUS_ZERO ;
      int isup ;
      if (!s6_svstatus_read(argv[i], &st)) strerr_diefu1sys(111, "s6_svstatus_read") ;
      isup = !!st.pid ;
      if (re[0] == 'U' && isup)
      {
        unsigned int len = str_len(argv[i]) ;
        char s[len + 1 + sizeof(S6_SUPERVISE_READY_FILENAME)] ;
        byte_copy(s, len, argv[i]) ;
        s[len] = '/' ;
        byte_copy(s + len + 1, sizeof(S6_SUPERVISE_READY_FILENAME), S6_SUPERVISE_READY_FILENAME) ;
        if (access(s, F_OK) < 0)
        {
          if (errno == ENOENT) isup = 0 ;
          else strerr_warnwu2sys("check ", s) ;
        }
      }
      bitarray_poke(states, i, isup) ;
    }

    pid = child_spawn0(argv[argc1 + 1], argv + argc1 + 1, envp) ;
    if (!pid) strerr_diefu2sys(111, "spawn ", argv[argc1 + 1]) ;

    for (;;)
    {
      register int r ;
      if (check(states, argc1, wantup, or)) break ;
      r = iopause_g(x, 2, &deadline) ;
      if (r < 0) strerr_diefu1sys(111, "iopause") ;
      else if (!r) strerr_dief1x(1, "timed out") ;

      if (x[0].revents & IOPAUSE_READ) handle_signals() ;
      if (x[1].revents & IOPAUSE_READ)
      {
        if (ftrigr_update(&a) < 0) strerr_diefu1sys(111, "ftrigr_update") ;
        for (i = 0 ; i < (unsigned int)argc1 ; i++)
        {
          char what ;
          register int r = ftrigr_check(&a, list[i], &what) ;
          if (r < 0) strerr_diefu1sys(111, "ftrigr_check") ;
          if (r) bitarray_poke(states, i, what == re[0]) ;
        }
      } 
    }
  }
  return 0 ;
}
