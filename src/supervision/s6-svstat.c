/* ISC license. */

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <skalibs/uint64.h>
#include <skalibs/uint.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-svstat servicedir"

int main (int argc, char const *const *argv)
{
  s6_svstatus_t status ;
  char fmt[UINT_FMT] ;
  int isup, normallyup ;
  PROG = "s6-svstat" ;
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  argv++ ; argc-- ;
  if (!s6_svstatus_read(*argv, &status))
    strerr_diefu2sys(111, "read status for ", *argv) ;

  {
    struct stat st ;
    unsigned int dirlen = str_len(*argv) ;
    char fn[dirlen + 6] ;
    byte_copy(fn, dirlen, *argv) ;
    byte_copy(fn + dirlen, 6, "/down") ;
    if (stat(fn, &st) == -1)
      if (errno != ENOENT) strerr_diefu2sys(111, "stat ", fn) ;
      else normallyup = 1 ;
    else normallyup = 0 ;
  }

  tain_now_g() ;
  if (tain_future(&status.stamp)) tain_copynow(&status.stamp) ;
  tain_sub(&status.stamp, &STAMP, &status.stamp) ;

  isup = status.pid && !status.flagfinishing ;
  if (isup)
  {
    buffer_putnoflush(buffer_1small,"up (pid ", 8) ;
    buffer_putnoflush(buffer_1small, fmt, uint_fmt(fmt, status.pid)) ;
    buffer_putnoflush(buffer_1small, ") ", 2) ;
  }
  else buffer_putnoflush(buffer_1small, "down ", 5) ;

  buffer_putnoflush(buffer_1small, fmt, uint64_fmt(fmt, status.stamp.sec.x)) ;
  buffer_putnoflush(buffer_1small," seconds", 8) ;

  if (isup && !normallyup)
    buffer_putnoflush(buffer_1small, ", normally down", 15) ;
  if (!isup && normallyup)
    buffer_putnoflush(buffer_1small, ", normally up", 13) ;
  if (isup && status.flagpaused)
    buffer_putnoflush(buffer_1small, ", paused", 8) ;
  if (!isup && (status.flagwant == 'u'))
    buffer_putnoflush(buffer_1small, ", want up", 10) ;
  if (isup && (status.flagwant == 'd'))
    buffer_putnoflush(buffer_1small, ", want down", 12) ;

  if (buffer_putflush(buffer_1small, "\n", 1) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
  return 0 ;
}
