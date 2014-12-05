/* ISC license. */

#include <skalibs/strerr2.h>
#include <s6/ftrigw.h>

#define USAGE "s6-ftrig-notify fifodir message"

int main (int argc, char const *const *argv)
{
  char const *p ;
  PROG = "s6-ftrig-notify" ;
  if (argc < 3) strerr_dieusage(100, USAGE) ;
  p = argv[2] ;
  for (; *p ; p++)
  {
    if (ftrigw_notify(argv[1], *p) == -1)
      strerr_diefu2sys(111, "notify ", argv[1]) ;
  }
  return 0 ;
}
