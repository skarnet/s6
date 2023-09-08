/* ISC license. */

#include <skalibs/sysdeps.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <skalibs/uint16.h>
#include <skalibs/fmtscan.h>
#include <skalibs/djbunix.h>

#include <s6/ucspiserver.h>

#ifdef SKALIBS_HASPOSIXSPAWN

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <spawn.h>

#include <skalibs/config.h>
#include <skalibs/env.h>

#ifdef SKALIBS_HASPOSIXSPAWNEARLYRETURN
extern pid_t child_spawn_workaround (pid_t, int const *) ;  /* XXX: non-public skalibs function */
#endif

pid_t s6_ucspiserver_spawn (int fd, char const *const *argv, char const *const *envp, size_t envlen, char const *modifs, size_t modiflen, size_t modifn)
{
  pid_t pid ;
  posix_spawn_file_actions_t actions ;
  posix_spawnattr_t attr ;
  sigset_t set ;
  int e ;
  int nopath = !getenv("PATH") ;
#ifdef SKALIBS_HASPOSIXSPAWNEARLYRETURN
  int p[2] ;
#endif
  char const *newenvp[envlen + modifn + 1] ;

#ifdef SKALIBS_HASPOSIXSPAWNEARLYRETURN
  if (pipecoe(p) == -1) return 0 ;
#endif
  e = posix_spawnattr_init(&attr) ;
  if (e) goto err ;
  sigemptyset(&set) ;
  e = posix_spawnattr_setsigmask(&attr, &set) ;
  if (e) goto errattr ;
  e = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGMASK) ;
  if (e) goto errattr ;
  e = posix_spawn_file_actions_init(&actions) ;
  if (e) goto errattr ;
  e = posix_spawn_file_actions_adddup2(&actions, fd, 0) ;
  if (e) goto erractions ;
  e = posix_spawn_file_actions_adddup2(&actions, fd, 1) ;
  if (e) goto erractions ;
  if (fd > 1)
  {
    e = posix_spawn_file_actions_addclose(&actions, fd) ;
    if (e) goto erractions ;
  }

  env_mergen(newenvp, envlen + modifn + 1, envp, envlen, modifs, modiflen, modifn) ;
  if (nopath && (setenv("PATH", SKALIBS_DEFAULTPATH, 0) < 0)) { e = errno ; goto erractions ; }
  e = posix_spawnp(&pid, argv[0], &actions, &attr, (char *const *)argv, (char *const *)newenvp) ;
  if (nopath) unsetenv("PATH") ;
  if (e) goto erractions ;

  posix_spawn_file_actions_destroy(&actions) ;
  posix_spawnattr_destroy(&attr) ;
#ifdef SKALIBS_HASPOSIXSPAWNEARLYRETURN
  return child_spawn_workaround(pid, p) ;
#else
  return pid ;
#endif

 erractions:
  posix_spawn_file_actions_destroy(&actions) ;
 errattr:
  posix_spawnattr_destroy(&attr) ;
 err:
#ifdef SKALIBS_HASPOSIXSPAWNEARLYRETURN
  fd_close(p[1]) ;
  fd_close(p[0]) ;
#endif
  errno = e ;
  return 0 ;
}

#else

#include <skalibs/strerr.h>
#include <skalibs/selfpipe.h>
#include <skalibs/exec.h>

pid_t s6_ucspiserver_spawn (int fd, char const *const *argv, char const *const *envp, size_t envlen, char const *modifs, size_t modiflen, size_t modifn)
{
  pid_t pid = fork() ;
  if (pid == -1) return 0 ;
  if (!pid)
  {
    size_t proglen = strlen(PROG) ;
    char newprog[proglen + 9] ;
    memcpy(newprog, PROG, proglen) ;
    memcpy(newprog, " (child)", 9) ;
    PROG = newprog ;
    if ((fd_move(1, fd) == -1) || (fd_copy(0, 1) == -1))
      strerr_diefu1sys(111, "move fds") ;
    selfpipe_finish() ;
    xmexec_fn(argv, envp, envlen, modifs, modiflen, modifn) ;
  }
  return pid ;
}

#endif
