/* ISC license. */

#include <skalibs/strerr.h>
#include <s6/supervise.h>

#define USAGE "s6-svdt-clear servicedir"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  PROG = "s6-svdt-clear" ;
  if (argc < 2) dieusage() ;
  if (!s6_dtally_write(argv[1], 0, 0))
    strerr_diefu2sys(111, "clear death tally for service ", argv[1]) ;
  return 0 ;
}
