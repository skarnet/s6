/* ISC license. */

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <skalibs/uint64.h>
#include <skalibs/uint.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/sig.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-svstat [ -n ] servicedir"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  s6_svstatus_t status ;
  int flagnum = 0 ;
  int isup, normallyup ;
  char fmt[UINT_FMT] ;
  PROG = "s6-svstat" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "n", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'n' : flagnum = 1 ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) dieusage() ;

  if (!s6_svstatus_read(*argv, &status))
    strerr_diefu2sys(111, "read status for ", *argv) ;
  isup = s6_svc_ok(argv[0]) ;
  if (isup < 0) strerr_diefu2sys(111, "check ", argv[0]) ;
  if (!isup) strerr_diefu3x(1, "read status for ", argv[0], ": s6-supervise not running") ;

  tain_now_g() ;
  if (tain_future(&status.stamp)) tain_copynow(&status.stamp) ;

  {
    size_t dirlen = str_len(*argv) ;
    char fn[dirlen + 6] ;
    byte_copy(fn, dirlen, *argv) ;
    byte_copy(fn + dirlen, 6, "/down") ;
    if (access(fn, F_OK) < 0)
      if (errno != ENOENT) strerr_diefu2sys(111, "access ", fn) ;
      else normallyup = 1 ;
    else normallyup = 0 ;
  }

  isup = status.pid && !status.flagfinishing ;
  if (isup)
  {
    buffer_putnoflush(buffer_1small,"up (pid ", 8) ;
    buffer_putnoflush(buffer_1small, fmt, uint_fmt(fmt, status.pid)) ;
    buffer_putnoflush(buffer_1small, ") ", 2) ;
  }
  else
  {
    buffer_putnoflush(buffer_1small, "down (", 6) ;
    if (WIFSIGNALED(status.wstat))
    {
      buffer_putnoflush(buffer_1small, "signal ", 7) ;
      if (flagnum)
        buffer_putnoflush(buffer_1small, fmt, uint_fmt(fmt, WTERMSIG(status.wstat))) ;
      else
      {
        buffer_putnoflush(buffer_1small, "SIG", 3) ;
        buffer_putsnoflush(buffer_1small, sig_name(WTERMSIG(status.wstat))) ;
      }
    }
    else
    {
      buffer_putnoflush(buffer_1small, "exitcode ", 9) ;
      buffer_putnoflush(buffer_1small, fmt, uint_fmt(fmt, WEXITSTATUS(status.wstat))) ;
    }
    buffer_putnoflush(buffer_1small, ") ", 2) ;
  }

  tain_sub(&status.stamp, &STAMP, &status.stamp) ;
  buffer_putnoflush(buffer_1small, fmt, uint64_fmt(fmt, status.stamp.sec.x)) ;
  buffer_putnoflush(buffer_1small, " seconds", 8) ;

  if (isup && !normallyup)
    buffer_putnoflush(buffer_1small, ", normally down", 15) ;
  if (!isup && normallyup)
    buffer_putnoflush(buffer_1small, ", normally up", 13) ;
  if (isup && status.flagpaused)
    buffer_putnoflush(buffer_1small, ", paused", 8) ;
  if (!isup && status.flagwant)
    buffer_putnoflush(buffer_1small, ", want up", 10) ;
  if (isup && !status.flagwant)
    buffer_putnoflush(buffer_1small, ", want down", 12) ;

  if (status.flagready)
  {
    tain_sub(&status.readystamp, &STAMP, &status.readystamp) ;
    buffer_putnoflush(buffer_1small, ", ready ", 8) ;
    buffer_putnoflush(buffer_1small, fmt, uint64_fmt(fmt, status.readystamp.sec.x)) ;
    buffer_putnoflush(buffer_1small, " seconds", 8) ;
  }
  if (buffer_putflush(buffer_1small, "\n", 1) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
  return 0 ;
}
