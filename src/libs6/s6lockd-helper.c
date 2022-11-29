/* ISC license. */

#include <skalibs/allreadwrite.h>
#include <skalibs/strerr.h>

#include "s6lockd.h"

#define USAGE "s6lockd-helper r|w lockfile"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  char c ;
  PROG = "s6lockd-helper" ;
  if (argc < 3) dieusage() ;
  s6lockd_openandlock(argv[2], argv[1][0] == 'w', 0) ;
  if (fd_write(1, "!", 1) <= 0)
    strerr_diefu1sys(111, "write to stdout") ;
  if (fd_read(0, &c, 1) < 0)
    strerr_diefu1sys(111, "read from stdin") ;
  return 0 ;
}
