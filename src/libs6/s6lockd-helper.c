/* ISC license. */

#include <errno.h>

#include <skalibs/allreadwrite.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>

#define USAGE "s6lockd-helper lockfile"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  int fd, r ;
  char c ;
  PROG = "s6lockd-helper" ;
  if (argc < 3) dieusage() ;
  fd = open_create(argv[2]) ;
  if (fd < 0) strerr_diefu2sys(111, "open ", argv[1]) ;
  r = fd_lock(fd, argv[1][0] == 'w', 0) ;
  if (!r) errno = EBUSY ;
  if (r < 1) strerr_diefu2sys(111, "lock ", argv[2]) ;
  if (fd_write(1, "!", 1) <= 0)
    strerr_diefu1sys(111, "write to stdout") ;
  if (fd_read(0, &c, 1) < 0)
    strerr_diefu1sys(111, "read from stdin") ;
  return 0 ;
}
