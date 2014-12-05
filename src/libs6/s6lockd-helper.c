/* ISC license. */

#include <skalibs/allreadwrite.h>
#include <skalibs/strerr2.h>
#include <skalibs/env.h>
#include <skalibs/djbunix.h>

#define USAGE "s6lockd-helper lockfile"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  int fd ;
  char const *x = env_get2(envp, "S6LOCK_EX") ;
  char c ;
  PROG = "s6lockd-helper" ;
  if (argc < 2) dieusage() ;
  fd = open_create(argv[1]) ;
  if (fd < 0) strerr_diefu2sys(111, "open ", argv[1]) ;
  if (((x && *x) ? lock_ex(fd) : lock_sh(fd)) < 0)
    strerr_diefu2sys(111, "lock ", argv[1]) ;
  if (fd_write(1, "!", 1) <= 0)
    strerr_diefu1sys(111, "write to stdout") ;
  if (fd_read(0, &c, 1) < 0)
    strerr_diefu1sys(111, "read from stdin") ;
  return 0 ;
}
