/* ISC license. */

#include <skalibs/strerr.h>
#include <s6/supervise.h>

#define USAGE "s6-svok servicedir"

int main (int argc, char const *const *argv)
{
  int r ;
  PROG = "s6-svok" ;
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  r = s6_svc_ok(argv[1]) ;
  if (r < 0) strerr_diefu2sys(111, "check ", argv[1]) ;
  return !r ;
}
