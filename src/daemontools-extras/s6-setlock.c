/* ISC license. */

#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <skalibs/sgetopt.h>
#include <skalibs/strerr.h>
#include <skalibs/types.h>
#include <skalibs/tai.h>
#include <skalibs/sig.h>
#include <skalibs/alarm.h>
#include <skalibs/djbunix.h>
#include <skalibs/exec.h>

#include <s6/config.h>

#define USAGE "s6-setlock [ -r | -w ] [ -n | -N ] [ -t timeout ] [ -d fd ] lockfile prog..."
#define dieusage() strerr_dieusage(100, USAGE)

static char const *file ;

static void sigalrm_handler (int sig)
{
  (void)sig ;
  strerr_dief3x(1, "lock ", file, ": timed out") ;
}

int main (int argc, char const *const *argv)
{
  unsigned int nb = 0, ex = 1 ;
  unsigned int timeout = 0 ;
  int dest = -1 ;
  int fd ;
  int r ;
  PROG = "s6-setlock" ;
  
  for (;;)
  {
    int opt = lgetopt(argc, argv, "nNrwt:d:") ;
    if (opt == -1) break ;
    switch (opt)
    {
      case 'n' : nb = 1 ; break ;
      case 'N' : nb = 0 ; break ;
      case 'r' : ex = 0 ; break ;
      case 'w' : ex = 1 ; break ;
      case 't' : if (!uint0_scan(subgetopt_here.arg, &timeout)) dieusage() ; break ;
      case 'd' : { unsigned int u ; if (!uint0_scan(subgetopt_here.arg, &u)) dieusage() ; dest = u ; break ; }
      default : dieusage() ;
    }
  }
  argc -= subgetopt_here.ind ; argv += subgetopt_here.ind ;
  if (argc < 2) dieusage() ;
  file = argv[0] ;

  if (ex)
  {
    fd = open_create(file) ;
    if (fd == -1) strerr_diefu3sys(111, "open ", file, " for writing") ;
  }
  else
  {
    fd = open_read(file) ;
    if (fd == -1)
    {
      if (errno != ENOENT) strerr_diefu3sys(111, "open ", file, " for reading") ;
      fd = open_create(file) ;
      if (fd == -1) strerr_diefu2sys(111, "create ", file) ;
      fd_close(fd) ;
      fd = open_read(file) ;
      if (fd == -1) strerr_diefu3sys(111, "open ", file, " for reading") ;
    }
  }

  if (timeout)
  {
    tain tto ;
    tain_from_millisecs(&tto, timeout) ;
    if (!sig_catch(SIGALRM, &sigalrm_handler))
      strerr_diefu1sys(111, "set SIGALRM handler") ;
    if (!alarm_timeout(&tto))
      strerr_diefu1sys(111, "set timer") ;
  }
  r = fd_lock(fd, ex, nb) ;
  if (timeout) alarm_disable() ;

  if (!r) errno = EBUSY ;
  if (r < 1) strerr_diefu2sys(1, "lock ", file) ;

  if (dest >= 0 && fd_move(dest, fd) == -1)
    strerr_diefu1sys(111, "move lock descriptor") ;
  xexec(argv+1) ;
}
