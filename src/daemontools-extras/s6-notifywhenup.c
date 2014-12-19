/* ISC license. */

#include <unistd.h>
#include <errno.h>
#include <skalibs/uint.h>
#include <skalibs/bytestr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <s6/ftrigw.h>

#define USAGE "s6-notifywhenup [ -d fd ] [ -e fifodir ] [ -f ] [ -t timeout ] prog..."
#define dieusage() strerr_dieusage(100, USAGE)

static int run_child (int fd, char const *fifodir, unsigned int timeout)
{
  char dummy[4096] ;
  iopause_fd x = { .fd = fd, .events = IOPAUSE_READ } ;
  tain_t deadline ;
  if (!tain_now_g()) strerr_diefu1sys(111, "tain_now") ;
  if (timeout) tain_from_millisecs(&deadline, timeout) ;
  else deadline = tain_infinite_relative ;
  tain_add_g(&deadline, &deadline) ;
  for (;;)
  {
    register int r = iopause_g(&x, 1, &deadline) ;
    if (r < 0) strerr_diefu1sys(111, "iopause") ;
    if (!r) return 99 ;
    r = sanitize_read(fd_read(fd, dummy, 4096)) ;
    if (r < 0)
      if (errno == EPIPE) return 0 ;
      else strerr_diefu1sys(111, "read from parent") ;
    else if (r)
      if (byte_chr(dummy, r, '\n') < r) break ;
  }
  ftrigw_notify(fifodir, 'U') ;
  return 0 ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{
  unsigned int fd = 1 ;
  char const *fifodir = "event" ;
  int df = 1 ;
  unsigned int timeout = 0 ;
  PROG = "s6-notifywhenup" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "d:e:ft:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'd' : if (!uint0_scan(l.arg, &fd)) dieusage() ; break ;
        case 'e' : fifodir = l.arg ; break ;
        case 'f' : df = 0 ; break ;
        case 't' : if (!uint0_scan(l.arg, &timeout)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) dieusage() ;

  {
    int p[2] ;
    pid_t pid ;
    if (pipe(p) < 0) strerr_diefu1sys(111, "pipe") ;
    pid = df ? doublefork() : fork() ;
    if (pid < 0) strerr_diefu1sys(111, df ? "doublefork" : "fork") ;
    else if (!pid)
    {
      PROG = "s6-notifywhenup (child)" ;
      fd_close(p[1]) ;
      return run_child(p[0], fifodir, timeout) ;
    }
    fd_close(p[0]) ;
    if (fd_move((int)fd, p[1]) < 0) strerr_diefu1sys(111, "fd_move") ;
  }
  pathexec_run(argv[0], argv, envp) ;
  strerr_dieexec(111, argv[1]) ;
}
