/* ISC license. */

#include <skalibs/nonposix.h>
#include <sys/socket.h>
#include <errno.h>
#include <signal.h>
#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/sgetopt.h>
#include <skalibs/error.h>
#include <skalibs/iobuffer.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>

#define USAGE "s6-ioconnect [ -t timeout ] [ -r fdr ] [ -w fdw ] [ -0 ] [ -1 ] [ -6 ] [ -7 ]"
#define dieusage() strerr_dieusage(100, USAGE)


typedef struct ioblah_s ioblah_t, *ioblah_t_ref ;
struct ioblah_s
{
  unsigned int fd ;
  unsigned int xindex ;
  unsigned int flagsocket : 1 ;
  unsigned int flagopen : 1 ;
} ;

static ioblah_t a[2][2] = { { { 0, 4, 0, 1 }, { 7, 4, 0, 1 } }, { { 6, 4, 0, 1 }, { 1, 4, 0, 1 } } } ;
static iobuffer b[2] ;
static iopause_fd x[5] = { { -1, IOPAUSE_READ, 0 } } ;

static void closeit (unsigned int i, unsigned int j)
{
  if (a[i][j].flagsocket)
  {
    if ((shutdown(a[i][j].fd, j) < 0) && (errno != ENOTSOCK) && (errno != ENOTCONN))
      strerr_warnwu4sys("shutdown ", i ? "incoming" : "outgoing", " socket for ", j ? "writing" : "reading") ;
  }
  fd_close(a[i][j].fd) ;
  a[i][j].flagopen = 0 ;
  a[i][j].xindex = 5 ;
}

static inline void finishit (unsigned int i)
{
  closeit(i, 1) ;
  iobuffer_finish(&b[i]) ;
}

static void handle_signals (void)
{
  for (;;)
  {
    switch (selfpipe_read())
    {
      case -1 : strerr_diefu1sys(111, "selfpipe_read") ;
      case 0 : return ;
      case SIGTERM :
      {
        if (a[0][0].xindex < 5) x[a[0][0].xindex].revents |= IOPAUSE_EXCEPT ;
        if (a[1][0].xindex < 5) x[a[1][0].xindex].revents |= IOPAUSE_EXCEPT ;
        break ;
      }
      default :
        strerr_dief1x(101, "internal error: inconsistent signal state. Please submit a bug-report.") ;
    }
  }
}

int main (int argc, char const *const *argv)
{
  tain_t tto ;
  unsigned int i, j ;
  PROG = "s6-ioconnect" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    unsigned int t = 0 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "0167t:r:w:", &l) ;
      if (opt < 0) break ;
      switch (opt)
      {
        case '0' : a[0][0].flagsocket = 1 ; break ;
        case '1' : a[1][1].flagsocket = 1 ; break ;
        case '6' : a[1][0].flagsocket = 1 ; break ;
        case '7' : a[0][1].flagsocket = 1 ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        case 'r' : if (!uint0_scan(l.arg, &a[1][0].fd)) dieusage() ; break ;
        case 'w' : if (!uint0_scan(l.arg, &a[0][1].fd)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    if (t) tain_from_millisecs(&tto, t) ; else tto = tain_infinite_relative ;
    argc -= l.ind ; argv += l.ind ;
  }
  if ((a[0][1].fd < 3) || (a[1][0].fd < 3)) dieusage() ;
  for (i = 0 ; i < 2 ; i++)
  {
    for (j = 0 ; j < 2 ; j++)
      if (ndelay_on(a[i][j].fd) == -1) strerr_diefu1sys(111, "ndelay_on") ;
    if (!iobuffer_init(&b[i], a[i][0].fd, a[i][1].fd)) strerr_diefu1sys(111, "iobuffer_init") ;
  }
  if (sig_ignore(SIGPIPE) == -1) strerr_diefu1sys(111, "sig_ignore") ;
  tain_now_set_stopwatch_g() ;
  x[0].fd = selfpipe_init() ;
  if (x[0].fd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
  if (selfpipe_trap(SIGTERM) < 0)
    strerr_diefu1sys(111, "trap SIGTERM") ;

  for (;;)
  {
    tain_t deadline ;
    unsigned int xlen = 1 ;
    int r ;

    tain_add_g(&deadline, iobuffer_isempty(&b[0]) && iobuffer_isempty(&b[1]) ? &tto : &tain_infinite_relative) ;
    for (i = 0 ; i < 2 ; i++)
    {
      a[i][0].xindex = 5 ;
      if (a[i][0].flagopen && iobuffer_isreadable(&b[i]))
      {
        x[xlen].fd = a[i][0].fd ;
        x[xlen].events = IOPAUSE_READ ;
        a[i][0].xindex = xlen++ ;
      }
      a[i][1].xindex = 5 ;
      if (a[i][1].flagopen)
      {
        x[xlen].fd = a[i][1].fd ;
        x[xlen].events = IOPAUSE_EXCEPT | (iobuffer_isempty(&b[i]) ? 0 : IOPAUSE_WRITE) ;
        a[i][1].xindex = xlen++ ;
      }
    }
    if (xlen <= 1) break ;

    r = iopause_g(x, xlen, &deadline) ;
    if (r < 0) strerr_diefu1sys(111, "iopause") ;
    else if (!r) return 1 ;

    if (x[0].revents & IOPAUSE_READ) handle_signals() ;

    for (i = 0 ; i < 2 ; i++) if (a[i][1].xindex < 5)
    {
      if (x[a[i][1].xindex].revents & IOPAUSE_WRITE)
      {
        if (!iobuffer_flush(&b[i]))
        {
          if (!error_isagain(errno)) x[a[i][1].xindex].revents |= IOPAUSE_EXCEPT ;
        }
        else if (!a[i][0].flagopen) finishit(i) ;
      }
      if (x[a[i][1].xindex].revents & IOPAUSE_EXCEPT)
      {
        if (!iobuffer_isempty(&b[i]))
        {
          iobuffer_flush(&b[i]) ; /* sets errno */
          strerr_warnwu3sys("write ", i ? "incoming" : "outgoing", " data") ;
        }
        closeit(i, 0) ; finishit(i) ;
      }
    }

    for (i = 0 ; i < 2 ; i++) if (a[i][0].xindex < 5)
    {
      if (x[a[i][0].xindex].revents & IOPAUSE_READ)
      {
        if (sanitize_read(iobuffer_fill(&b[i])) < 0)
        {
          if (errno != EPIPE) strerr_warnwu3sys("read ", i ? "incoming" : "outgoing", " data") ;
          x[a[i][0].xindex].revents |= IOPAUSE_EXCEPT ;
        }
      }
      if (x[a[i][0].xindex].revents & IOPAUSE_EXCEPT)
      {
        closeit(i, 0) ;
        if (iobuffer_isempty(&b[i])) finishit(i) ;
      }
    }
  }
  return 0 ;
}
