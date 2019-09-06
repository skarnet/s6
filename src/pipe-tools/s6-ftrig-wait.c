/* ISC license. */

#include <stdint.h>
#include <errno.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/sgetopt.h>
#include <skalibs/types.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <s6/ftrigr.h>

#define USAGE "s6-ftrig-wait [ -t timeout ] fifodir regexp"

int main (int argc, char const *const *argv)
{
  tain_t deadline, tto ;
  ftrigr_t a = FTRIGR_ZERO ;
  uint16_t id ;
  char pack[2] = " \n" ;
  PROG = "s6-ftrig-wait" ;
  {
    unsigned int t = 0 ;
    for (;;)
    {
      int opt = subgetopt(argc, argv, "t:") ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 't' : if (uint0_scan(subgetopt_here.arg, &t)) break ;
        default : strerr_dieusage(100, USAGE) ;
      }
    }
    if (t) tain_from_millisecs(&tto, t) ;
    else tto = tain_infinite_relative ;
    argc -= subgetopt_here.ind ; argv += subgetopt_here.ind ;
  }
  if (argc < 2) strerr_dieusage(100, USAGE) ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&deadline, &tto) ;

  if (!ftrigr_startf_g(&a, &deadline)) strerr_diefu1sys(111, "ftrigr_startf") ;
  id = ftrigr_subscribe_g(&a, argv[0], argv[1], 0, &deadline) ;
  if (!id) strerr_diefu4sys(111, "subscribe to ", argv[0], " with regexp ", argv[1]) ;
  if (ftrigr_wait_or_g(&a, &id, 1, &deadline, &pack[0]) == -1)
    strerr_diefu2sys((errno == ETIMEDOUT) ? 1 : 111, "match regexp on ", argv[1]) ;
  if (allwrite(1, pack, 2) < 2) strerr_diefu1sys(111, "write to stdout") ;
  return 0 ;
}
