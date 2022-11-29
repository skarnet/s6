/* ISC license. */

#include <skalibs/strerr.h>
#include <s6/ftrigw.h>

#define USAGE "s6-ftrig-notify fifodir message"

int main (int argc, char const *const *argv)
{
  PROG = "s6-ftrig-notify" ;
  if (argc < 3) strerr_dieusage(100, USAGE) ;
  if (ftrigw_notifys(argv[1], argv[2]) < 0)
    strerr_diefu2sys(111, "notify ", argv[1]) ;
  return 0 ;
}
