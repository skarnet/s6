/* ISC license. */

#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

#include <skalibs/strerr2.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/djbunix.h>
#include <skalibs/exec.h>

#define USAGE "s6-fghack prog..."

int main (int argc, char const *const *argv)
{
  int p[2] ;
  int pcoe[2] ;
  pid_t pid ;
  char dummy ;
  PROG = "s6-fghack" ;
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  if (pipe(p) < 0) strerr_diefu1sys(111, "create hackpipe") ;
  if (pipe(pcoe) < 0) strerr_diefu1sys(111, "create coepipe") ;

  switch (pid = fork())
  {
    case -1 : strerr_diefu1sys(111, "fork") ;
    case 0 :
    {
      int i = 0 ;
      close(p[0]) ;
      close(pcoe[0]) ;
      if (coe(pcoe[1]) < 0) _exit(111) ;
      for (; i < 30 ; i++) dup(p[1]) ; /* hack. gcc's warning is justified. */
      exec(argv+1) ;
      i = errno ;
      if (fd_write(pcoe[1], "", 1) < 1) _exit(111) ;
      _exit(i) ;
    }
  }

  close(p[1]) ;
  close(pcoe[1]) ;

  switch (fd_read(pcoe[0], &dummy, 1))
  {
    case -1 : strerr_diefu1sys(111, "read on coepipe") ;
    case 1 :
    {
      int wstat ;
      if (wait_pid(pid, &wstat) < 0) strerr_diefu1sys(111, "wait_pid") ;
      errno = WEXITSTATUS(wstat) ;
      strerr_dieexec(111, argv[1]) ;
    }
  }

  fd_close(pcoe[0]) ;

  p[1] = fd_read(p[0], &dummy, 1) ;
  if (p[1] < 0) strerr_diefu1sys(111, "read on hackpipe") ;
  if (p[1]) strerr_dief2x(102, argv[1], " wrote on hackpipe") ;

  {
    int wstat ;
    if (wait_pid(pid, &wstat) < 0) strerr_diefu1sys(111, "wait_pid") ;
    if (WIFSIGNALED(wstat)) strerr_dief2x(111, argv[2], " crashed") ;
    return WEXITSTATUS(wstat) ;
  }
}
