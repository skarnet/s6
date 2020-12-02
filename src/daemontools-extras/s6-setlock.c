/* ISC license. */

#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <skalibs/allreadwrite.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/types.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/exec.h>

#include <s6/config.h>

#define USAGE "s6-setlock [ -r | -w ] [ -n | -N | -t timeout ] lockfile prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  unsigned int nb = 0, ex = 1 ;
  unsigned int timeout = 0 ;
  PROG = "s6-setlock" ;
  for (;;)
  {
    int opt = subgetopt(argc, argv, "nNrwt:") ;
    if (opt == -1) break ;
    switch (opt)
    {
      case 'n' : nb = 1 ; break ;
      case 'N' : nb = 0 ; break ;
      case 'r' : ex = 0 ; break ;
      case 'w' : ex = 1 ; break ;
      case 't' : if (!uint0_scan(subgetopt_here.arg, &timeout)) dieusage() ; nb = 2 ; break ;
      default : dieusage() ;
    }
  }
  argc -= subgetopt_here.ind ; argv += subgetopt_here.ind ;
  if (argc < 2) dieusage() ;

  if (nb < 2)
  {
    int r ;
    int fd = open_create(argv[0]) ;
    if (fd < 0) strerr_diefu2sys(111, "open_create ", argv[0]) ;
    r = fd_lock(fd, ex, nb) ;
    if (!r) errno = EBUSY ;
    if (r < 1) strerr_diefu2sys(1, "lock ", argv[0]) ;
  }
  else
  {
    char const *cargv[4] = { "s6lockd-helper", ex ? "w" : "r", argv[0], 0 } ;
    char const *nullenv = { 0 } ;
    iopause_fd x = { .events = IOPAUSE_READ } ;
    tain_t deadline ;
    int p[2] = { 0, 1 } ;
    pid_t pid ;
    char c ;
    tain_now_set_stopwatch_g() ;
    tain_from_millisecs(&deadline, timeout) ;
    tain_add_g(&deadline, &deadline) ;
    pid = child_spawn2(S6_LIBEXECPREFIX "s6lockd-helper", cargv, &nullenv, p) ;
    if (!pid) strerr_diefu2sys(111, "spawn ", S6_LIBEXECPREFIX "s6lockd-helper") ;
    x.fd = p[0] ;
    for (;;)
    {
      ssize_t rr ;
      int r = iopause_g(&x, 1, &deadline) ;
      if (r < 0) strerr_diefu1sys(111, "iopause") ;
      if (!r)
      {
        kill(pid, SIGTERM) ;
        errno = ETIMEDOUT ;
        strerr_diefu1sys(1, "acquire lock") ;
      }
      rr = sanitize_read(fd_read(p[0], &c, 1)) ;
      if (rr < 0) strerr_diefu1sys(111, "read ack from helper") ;
      if (rr) break ;
    }
    if (c != '!') strerr_dief1x(111, "helper sent garbage ack") ;
    fd_close(p[0]) ;
    if (uncoe(p[1]) < 0) strerr_diefu1sys(111, "uncoe fd to helper") ;
  }
  xexec(argv+1) ;
}
