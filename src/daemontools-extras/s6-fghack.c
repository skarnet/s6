/* ISC license. */

#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

#include <skalibs/strerr.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/djbunix.h>
#include <skalibs/cspawn.h>

#define USAGE "s6-fghack prog..."

#define N 30

int main (int argc, char const *const *argv, char const *const *envp)
{
  int p[2] ;
  int fds[N] ;
  pid_t pid ;
  cspawn_fileaction fa = { .type = CSPAWN_FA_CLOSE } ;
  char c ;

  PROG = "s6-fghack" ;
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  if (pipe(p) == -1) strerr_diefu1sys(111, "create hackpipe") ;
  for (size_t i = 0 ; i < N ; i++)
    fds[i] = dup(p[1]) ;
  fa.x.fd = p[0] ;
  pid = cspawn(argv[1], argv + 1, envp, 0, &fa, 1) ;
  if (!pid) strerr_diefu2sys(111, "spawn ", argv[1]) ;
  close(p[1]) ;
  for (size_t i = 0 ; i < N ; i++) close(fds[i]) ;

  p[1] = fd_read(p[0], &c, 1) ;
  if (p[1] == -1) strerr_diefu1sys(111, "read on hackpipe") ;
  if (p[1]) strerr_dief2x(102, argv[1], " wrote on hackpipe") ;
  if (wait_pid(pid, &p[1]) < 0) strerr_diefu1sys(111, "wait_pid") ;
  return wait_estatus(p[1]) ;
}
