/* ISC license. */

#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/sig.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-svstat [ -uwNrpest | -o up,wantedup,normallyup,ready,paused,pid,exitcode,signal,signum,updownsince,readysince,updownfor,readyfor ] [ -n ] servicedir"
#define dieusage() strerr_dieusage(100, USAGE)

#define MAXFIELDS 16
#define checkfields() if (n >= MAXFIELDS) strerr_dief1x(100, "too many option fields")

static int normallyup ;

typedef void prfunc_t (buffer *, s6_svstatus_t const *) ;
typedef prfunc_t * prfunc_t_ref ;

typedef struct funcmap_s funcmap_t ;
struct funcmap_s
{
  char const *s ;
  prfunc_t_ref f ;
} ;

static void pr_up (buffer *b, s6_svstatus_t const *st)
{
  buffer_putsnoflush(b, st->pid && !st->flagfinishing ? st->flagthrottled ? "throttled" : "true" : "false") ;
}

static void pr_wantedup (buffer *b, s6_svstatus_t const *st)
{
  buffer_putsnoflush(b, st->flagwantup ? "true" : "false") ;
}

static void pr_ready (buffer *b, s6_svstatus_t const *st)
{
  buffer_putsnoflush(b, st->pid && st->flagready ? "true" : "false") ;
}

static void pr_paused (buffer *b, s6_svstatus_t const *st)
{
  buffer_putsnoflush(b, st->flagpaused ? "true" : "false") ;
}

static void pr_pid (buffer *b, s6_svstatus_t const *st)
{
  if (st->pid && !st->flagfinishing)
  {
    char fmt[PID_FMT] ;
    buffer_putnoflush(b, fmt, pid_fmt(fmt, st->pid)) ;
  }
  else buffer_putsnoflush(b, "-1") ;
}

static void pr_tain (buffer *b, tain_t const *a)
{
  char fmt[TIMESTAMP] ;
  buffer_putnoflush(b, fmt, timestamp_fmt(fmt, a)) ;
}

static void pr_stamp (buffer *b, s6_svstatus_t const *st)
{
  pr_tain(b, &st->stamp) ;
}

static void pr_readystamp (buffer *b, s6_svstatus_t const *st)
{
  pr_tain(b, &st->readystamp) ;
}

static void pr_seconds (buffer *b, tain_t const *a)
{
  tain_t d ;
  char fmt[UINT64_FMT] ;
  tain_sub(&d, &STAMP, a) ;
  buffer_putnoflush(b, fmt, uint64_fmt(fmt, tai_sec(tain_secp(&d)))) ;
}

static void pr_upseconds (buffer *b, s6_svstatus_t const *st)
{
  pr_seconds(b, &st->stamp) ;
}

static void pr_readyseconds (buffer *b, s6_svstatus_t const *st)
{
  pr_seconds(b, &st->readystamp) ;
}

static void pr_exitcode (buffer *b, s6_svstatus_t const *st)
{
  int e = st->pid && !st->flagfinishing ? -1 :
          WIFEXITED(st->wstat) ? WEXITSTATUS(st->wstat) : -1 ;
  char fmt[INT_FMT] ;
  buffer_putnoflush(b, fmt, int_fmt(fmt, e)) ;
}

static void pr_signum (buffer *b, s6_svstatus_t const *st)
{
  int e = st->pid && !st->flagfinishing ? -1 :
            WIFSIGNALED(st->wstat) ? WTERMSIG(st->wstat) : -1 ;
  char fmt[INT_FMT] ;
  buffer_putnoflush(b, fmt, int_fmt(fmt, e)) ;
}

static void pr_signal (buffer *b, s6_svstatus_t const *st)
{
  int e = st->pid && !st->flagfinishing ? -1 :
            WIFSIGNALED(st->wstat) ? WTERMSIG(st->wstat) : -1 ;
  if (e == -1) buffer_putsnoflush(b, "NA") ;
  else
  {
    buffer_putsnoflush(b, "SIG") ;
    buffer_putsnoflush(b, sig_name(e)) ;
  }
}

static void pr_normallyup (buffer *b, s6_svstatus_t const *st)
{
  buffer_putsnoflush(b, normallyup ? "true" : "false") ;
  (void)st ;
}

static funcmap_t const fmtable[] =
{
  { .s = "up", .f = &pr_up },
  { .s = "wantedup", .f = &pr_wantedup },
  { .s = "normallyup", .f = &pr_normallyup },
  { .s = "ready", .f = &pr_ready },
  { .s = "paused", .f = &pr_paused },
  { .s = "pid", .f = &pr_pid },
  { .s = "exitcode", .f = &pr_exitcode },
  { .s = "signal", .f = &pr_signal },
  { .s = "signum", .f = &pr_signum },
  { .s = "updownsince", .f = &pr_stamp },
  { .s = "readysince", .f = &pr_readystamp },
  { .s = "updownfor", .f = &pr_upseconds },
  { .s = "readyfor", .f = &pr_readyseconds },
  { .s = 0, .f = 0 }
} ;


static unsigned int parse_options (char const *arg, prfunc_t_ref *fields, unsigned int n)
{
  while (*arg)
  {
    size_t pos = str_chr(arg, ',') ;
    funcmap_t const *p = fmtable ;
    if (!pos) strerr_dief1x(100, "invalid null option field") ;
    for (; p->s ; p++) if (!strncmp(arg, p->s, pos)) break ;
    if (!p->s)
    {
      char blah[pos+1] ;
      memcpy(blah, arg, pos) ;
      blah[pos] = 0 ;
      strerr_dief2x(100, "invalid option field: ", blah) ;
    }
    checkfields() ;
    fields[n++] = p->f ;
    arg += pos ; if (*arg) arg++ ;
  }
  return n ;
}

static void legacy (s6_svstatus_t *st, int flagnum)
{
  s6_svstatus_t status = *st ;
  int isup = status.pid && !status.flagfinishing ;
  char fmt[UINT64_FMT] ;

  if (isup)
  {
    buffer_putnoflush(buffer_1small,"up (pid ", 8) ;
    buffer_putnoflush(buffer_1small, fmt, pid_fmt(fmt, status.pid)) ;
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

  if (isup && status.flagthrottled)
    buffer_putnoflush(buffer_1small, ", throttled", 11) ;
  if (isup && !normallyup)
    buffer_putnoflush(buffer_1small, ", normally down", 15) ;
  if (!isup && normallyup)
    buffer_putnoflush(buffer_1small, ", normally up", 13) ;
  if (isup && status.flagpaused)
    buffer_putnoflush(buffer_1small, ", paused", 8) ;
  if (!isup && status.flagwantup)
    buffer_putnoflush(buffer_1small, ", want up", 9) ;
  if (isup && !status.flagwantup)
    buffer_putnoflush(buffer_1small, ", want down", 11) ;

  if (status.flagready)
  {
    tain_sub(&status.readystamp, &STAMP, &status.readystamp) ;
    buffer_putnoflush(buffer_1small, ", ready ", 8) ;
    buffer_putnoflush(buffer_1small, fmt, uint64_fmt(fmt, status.readystamp.sec.x)) ;
    buffer_putnoflush(buffer_1small, " seconds", 8) ;
  }
}

int main (int argc, char const *const *argv)
{
  s6_svstatus_t status ;
  int flagnum = 0 ;
  prfunc_t_ref fields[MAXFIELDS] ;
  unsigned int n = 0 ;
  PROG = "s6-svstat" ;

  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "no:uwNrpest", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'n' : flagnum = 1 ; break ;
        case 'o' : n = parse_options(l.arg, fields, n) ; break ;
        case 'u' : checkfields() ; fields[n++] = &pr_up ; break ;
        case 'w' : checkfields() ; fields[n++] = &pr_wantedup ; break ;
        case 'N' : checkfields() ; fields[n++] = &pr_normallyup ; break ;
        case 'r' : checkfields() ; fields[n++] = &pr_ready ; break ;
        case 'p' : checkfields() ; fields[n++] = &pr_pid ; break ;
        case 'e' : checkfields() ; fields[n++] = &pr_exitcode ; break ;
        case 's' : checkfields() ; fields[n++] = &pr_signal ; break ;
        case 't' : checkfields() ; fields[n++] = &pr_upseconds ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) dieusage() ;
  fields[n] = 0 ;

  {
    int r = s6_svc_ok(argv[0]) ;
    if (r < 0) strerr_diefu2sys(111, "check ", argv[0]) ;
    if (!r) strerr_diefu3x(1, "read status for ", argv[0], ": s6-supervise not running") ;
  }
  if (!s6_svstatus_read(argv[0], &status))
    strerr_diefu2sys(111, "read status for ", argv[0]) ;

  tain_wallclock_read_g() ;
  if (tain_future(&status.stamp)) tain_copynow(&status.stamp) ;

  {
    size_t dirlen = strlen(*argv) ;
    char fn[dirlen + 6] ;
    memcpy(fn, *argv, dirlen) ;
    memcpy(fn + dirlen, "/down", 6) ;
    if (access(fn, F_OK) < 0)
      if (errno != ENOENT) strerr_diefu2sys(111, "access ", fn) ;
      else normallyup = 1 ;
    else normallyup = 0 ;
  }

  if (!n) legacy(&status, flagnum) ;
  else
  {
    unsigned int i = 0 ;
    for (; fields[i] ; i++)
    {
      (*fields[i])(buffer_1small, &status) ;
      buffer_putsnoflush(buffer_1small, " ") ;
    }
    buffer_unput(buffer_1small, 1) ;
  }

  if (buffer_putflush(buffer_1small, "\n", 1) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
  return 0 ;
}
