/* ISC license. */

#include <errno.h>
#include <skalibs/bytestr.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-svok servicedir"

int main (int argc, char const *const *argv)
{
  PROG = "s6-svok" ;
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  argv++ ; argc-- ;
  {
    int fd ;
    unsigned int dirlen = str_len(*argv) ;
    char fn[dirlen + 9 + sizeof(S6_SUPERVISE_CTLDIR)] ;
    byte_copy(fn, dirlen, *argv) ;
    fn[dirlen] = '/' ;
    byte_copy(fn + dirlen + 1, sizeof(S6_SUPERVISE_CTLDIR) - 1, S6_SUPERVISE_CTLDIR) ;
    byte_copy(fn + dirlen + sizeof(S6_SUPERVISE_CTLDIR), 9, "/control") ;
    fd = open_write(fn) ;
    if (fd < 0)
    {
      if ((errno == ENXIO) || (errno == ENOENT)) return 1 ;
      else strerr_diefu2sys(111, "open_write ", fn) ;
    }
  }
  return 0 ;
}
