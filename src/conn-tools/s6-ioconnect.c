/* ISC license. */

#include <errno.h>
#include <signal.h>
#include <sys/stat.h>

#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/sgetopt.h>
#include <skalibs/error.h>
#include <skalibs/buffer.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/strerr.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>

#define USAGE "s6-ioconnect [ -t timeout ] [ -r fdr ] [ -w fdw ]"
#define dieusage() strerr_dieusage(100, USAGE)

#define BSIZE 8192

typedef struct ioblah_s ioblah, *ioblah_ref ;
struct ioblah_s
{
  buffer b ;
  unsigned int xindex ;
  unsigned int flagsocket : 1 ;
} ;

static char buf[2][BSIZE] = { { '\0' }, { '\0' } } ;
static ioblah a[2][2] =
{
  {
    { .b = BUFFER_INIT(&buffer_read, 0, buf[0], BSIZE), .xindex = 5 },
    { .b = BUFFER_INIT(&buffer_write, 7, buf[0], BSIZE), .xindex = 5 }
  },
  {
    { .b = BUFFER_INIT(&buffer_read, 6, buf[1], BSIZE), .xindex = 5 },
    { .b = BUFFER_INIT(&buffer_write, 1, buf[1], BSIZE), .xindex = 5 }
  }
} ;
static iopause_fd x[5] = { [0] = { .fd = -1, .events = IOPAUSE_READ } } ;

static void closeit (unsigned int i, unsigned int j)
{
  int fd = buffer_fd(&a[i][j].b) ;
  if (a[i][j].flagsocket) fd_shutdown(fd, j) ;
  fd_close(fd) ;
  buffer_fd(&a[i][j].b) = -1 ;
  a[i][j].xindex = 5 ;
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

static int flushit (unsigned int i)
{
  int r = buffer_flush(&a[i][1].b) ;
  a[i][0].b.c.p = a[i][1].b.c.p ; /* XXX: abstraction leak */
  return r ;
}

static int fillit (unsigned int i)
{
  ssize_t r = sanitize_read(buffer_fill(&a[i][0].b)) ;
  a[i][1].b.c.n = a[i][0].b.c.n ; /* XXX: abstraction leak */
  return r >= 0 ;
}

int main (int argc, char const *const *argv)
{
  tain tto ;
  unsigned int i, j ;
  PROG = "s6-ioconnect" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    unsigned int t = 0 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "0167t:r:w:", &l) ;
      if (opt < 0) break ;
      switch (opt)
      {
        case '0' : break ;  /* these options are deprecated */
        case '1' : break ;  /* only there for compatibility */
        case '6' : break ;  /* flagsocket is autodetected now */
        case '7' : break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        case 'r' : if (!int0_scan(l.arg, &buffer_fd(&a[1][0].b))) dieusage() ; break ;
        case 'w' : if (!int0_scan(l.arg, &buffer_fd(&a[0][1].b))) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    if (t) tain_from_millisecs(&tto, t) ; else tto = tain_infinite_relative ;
    argc -= l.ind ; argv += l.ind ;
  }
  if ((buffer_fd(&a[0][1].b) < 3) || (buffer_fd(&a[1][0].b) < 3)) dieusage() ;
  for (i = 0 ; i < 2 ; i++) for (j = 0 ; j < 2 ; j++)
  {
    int fd = buffer_fd(&a[i][j].b) ;
    struct stat st ;
    if (fstat(fd, &st) == -1)
    {
      char fmt[INT_FMT] ;
      fmt[int_fmt(fmt, fd)] = 0 ;
      strerr_diefu2sys(111, "fstat fd ", fmt) ;
    }
    a[i][j].flagsocket = !!S_ISSOCK(st.st_mode) ;
    if (ndelay_on(fd) == -1)
    {
      char fmt[INT_FMT] ;
      fmt[int_fmt(fmt, fd)] = 0 ;
      strerr_diefu3sys(111, "set fd ", fmt, " non-blocking") ;
    }
  }
  if (!sig_ignore(SIGPIPE)) strerr_diefu1sys(111, "ignore SIGPIPE") ;
  tain_now_set_stopwatch_g() ;
  x[0].fd = selfpipe_init() ;
  if (x[0].fd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
  if (!selfpipe_trap(SIGTERM))
    strerr_diefu1sys(111, "trap SIGTERM") ;

  for (;;)
  {
    tain deadline ;
    unsigned int xlen = 1 ;
    tain_add_g(&deadline, buffer_isempty(&a[0][1].b) && buffer_isempty(&a[1][1].b) ? &tto : &tain_infinite_relative) ;

    for (i = 0 ; i < 2 ; i++)
    {
      if (buffer_fd(&a[i][0].b) >= 0 && buffer_isreadable(&a[i][0].b))
      {
        x[xlen].fd = buffer_fd(&a[i][0].b) ;
        x[xlen].events = IOPAUSE_READ ;
        a[i][0].xindex = xlen++ ;
      }
      else a[i][0].xindex = 5 ;
      if (buffer_fd(&a[i][1].b) >= 0)
      {
        x[xlen].fd = buffer_fd(&a[i][1].b) ;
        x[xlen].events = IOPAUSE_EXCEPT | (buffer_iswritable(&a[i][1].b) ? IOPAUSE_WRITE : 0) ;
        a[i][1].xindex = xlen++ ;
      }
      else a[i][1].xindex = 5 ;
    }
    if (xlen == 1) break ;

    {
      int r = iopause_g(x, xlen, &deadline) ;
      if (r < 0) strerr_diefu1sys(111, "iopause") ;
      else if (!r) return 1 ;
    }

    if (x[0].revents & IOPAUSE_READ) handle_signals() ;

    for (i = 0 ; i < 2 ; i++) if (a[i][1].xindex < 5)
    {
      int dead = 0 ;
      if (x[a[i][1].xindex].revents & IOPAUSE_EXCEPT)
      {
        if (!buffer_isempty(&a[i][1].b))
        {
          char fmt[INT_FMT] ;
          fmt[int_fmt(fmt, buffer_fd(&a[i][1].b))] = 0 ;
          flushit(i) ; /* sets errno */
          strerr_warnwu2sys("write to fd ", fmt) ;
        }
        dead = 1 ;
      }
      else if (x[a[i][1].xindex].revents & IOPAUSE_WRITE)
      {
        if (!flushit(i))
        {
          if (!error_isagain(errno)) dead = 1 ;
        }
        else if (buffer_fd(&a[i][0].b) == -1) dead = 1 ;
      }
      if (dead)
      {
        if (buffer_fd(&a[i][0].b) >= 0) closeit(i, 0) ;
        closeit(i, 1) ;
      }
    }

    for (i = 0 ; i < 2 ; i++) if (a[i][0].xindex < 5)
    {
      if (x[a[i][0].xindex].revents & (IOPAUSE_READ | IOPAUSE_EXCEPT))
      {
        if (!fillit(i))
        {
          if (errno != EPIPE)
          {
            char fmt[INT_FMT] ;
            fmt[int_fmt(fmt, buffer_fd(&a[i][0].b))] = 0 ;
            strerr_warnwu2sys("read from fd ", fmt) ;
          }
          closeit(i, 0) ;
          if (buffer_isempty(&a[i][1].b)) closeit(i, 1) ;
        }
      }
    }
  }
  return 0 ;
}
