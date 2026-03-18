/* ISC license. */

#include <sys/uio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include <skalibs/allreadwrite.h>
#include <skalibs/gol.h>
#include <skalibs/types.h>
#include <skalibs/strerr.h>
#include <skalibs/tai.h>
#include <skalibs/siovec.h>

#include <s6/ftrigr.h>

#define USAGE "s6-ftrig-wait [ -t timeout ] fifodir regexp"

enum gola_e
{
  GOLA_TIMEOUT,
  GOLA_N
} ;

int main (int argc, char const *const *argv)
{
  static gol_arg const rgola[GOLA_N] =
  {
    { .so = 't', .lo = "timeout", .i = GOLA_TIMEOUT },
  } ;
  char const *wgola[GOLA_N] = { 0 } ;
  tain deadline ;
  tain tto = TAIN_INFINITE_RELATIVE ;
  ftrigr a = FTRIGR_ZERO ;
  ftrigr_string fs ;
  uint32_t id ;
  unsigned int golc ;

  PROG = "s6-ftrig-wait" ;
  golc = gol_main(argc, argv, 0, 0, rgola, GOLA_N, 0, wgola) ;
  argc -= golc ; argv += golc ;
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  if (wgola[GOLA_TIMEOUT])
  {
    unsigned int t = 0 ;
    if (!uint0_scan(wgola[GOLA_TIMEOUT], &t)) 
      strerr_dief1x(100, "timeout must be an unsigned integer") ;    
    if (t) tain_from_millisecs(&tto, t) ;
  }

  if (!tain_now_set_stopwatch_g()) strerr_diefu1sys(111, "tain_now") ;
  tain_add_g(&deadline, &tto) ;

  if (!ftrigr_startf_g(&a, &deadline)) strerr_diefu1sys(111, "ftrigr_startf") ;
  if (!ftrigr_subscribe_g(&a, &id, 0, 0, argv[0], argv[1], &deadline))
    strerr_diefu4sys(111, "subscribe to ", argv[0], " with regexp ", argv[1]) ;
  if (ftrigr_wait_or_g(&a, &id, 1, &fs, &deadline) == -1)
    strerr_diefu2sys((errno == ETIMEDOUT) ? 99 : 111, "match regexp on ", argv[1]) ;

  struct iovec v[2] = { { .iov_base = fs.s, .iov_len = fs.len }, { .iov_base = "\n", .iov_len = 1 } } ;
  if (allwritev(1, v, 2) < siovec_len(v, 2)) strerr_diefu1sys(111, "write to stdout") ;
  _exit(0) ;
}
