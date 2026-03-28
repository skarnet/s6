/* ISC license. */

#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/envexec.h>
#include <skalibs/sig.h>
#include <skalibs/djbunix.h>

#define USAGE "s6-setsid [ -s | -b | -f | -g ] [ -i | -I | -q ] [ -d ctty ] prog..."
#define dieusage() strerr_dieusage(100, USAGE)

enum golb_e
{
  GOLB_PGRP = 0x01,
  GOLB_FG = 0x02,
  GOLB_GRAB = 0x04,
  GOLB_STRICT = 0x08,
  GOLB_SILENT = 0x10,
} ;

enum gola_e
{
  GOLA_CTTY,
  GOLA_N
} ;

int main (int argc, char const *const *argv)
{
  static gol_bool const rgolb[] =
  {
    { .so = 's', .lo = "session", .clear = GOLB_PGRP | GOLB_FG | GOLB_GRAB, .set = 0 },
    { .so = 0, .lo = "process-group", .clear = 0, .set = GOLB_PGRP },
    { .so = 'b', .lo = "background-process-group", .clear = GOLB_FG | GOLB_GRAB, .set = GOLB_PGRP },
    { .so = 0, .lo = "foreground", .clear = 0, .set = GOLB_FG },
    { .so = 'f', .lo = "foreground-process-group", .clear = GOLB_GRAB, .set = GOLB_PGRP | GOLB_FG },
    { .so = 0, .lo = "grab", .clear = 0, .set = GOLB_GRAB },
    { .so = 'g', .lo = "foreground-process-group-grab", .clear = 0, .set = GOLB_PGRP | GOLB_FG | GOLB_GRAB },
    { .so = 'I', .lo = "no-strict", .clear = GOLB_STRICT, .set = 0 },
    { .so = 'i', .lo = "strict", .clear = 0, .set = GOLB_STRICT },
    { .so = 'q', .lo = "quiet", .clear = 0, .set = GOLB_SILENT }
  } ;
  static gol_arg const rgola[] =
  {
    { .so = 'd', .lo = "ctty", .i = GOLA_CTTY },
  } ;
  char const *wgola[GOLA_N] = { 0 } ;
  uint64_t wgolb = 0 ;
  unsigned int ctty = 0 ;
  unsigned int golc ;
  PROG = "s6-setsid" ;

  golc = GOL_main(argc, argv, rgolb, rgola, &wgolb, wgola) ;
  argc -= golc ; argv += golc ;
  if (!argc) dieusage() ;
  if (wgola[GOLA_CTTY])
  {
    if (!uint0_scan(wgola[GOLA_CTTY], &ctty)) dieusage() ;
  }
  else if (wgolb & GOLB_PGRP & GOLB_FG)
  {
    int fd = openc_read("/dev/tty") ;
    if (fd == -1) strerr_diefu2sys(111, "open ", "/dev/tty") ;
    ctty = fd ;
  }

  if (wgolb & GOLB_PGRP)
  {
    if (setpgid(0, 0) == -1)
    {
      if (wgolb & GOLB_STRICT) strerr_diefu1sys(111, "setpgid") ;
      if (!(wgolb & GOLB_SILENT)) strerr_warnwu1sys("setpgid") ;
    }

    if (wgolb & GOLB_FG)
    {
      if (wgolb & GOLB_GRAB) sig_altignore(SIGTTOU) ;
      if (tcsetpgrp(ctty, getpid()) == -1)
      {
        if (wgolb & GOLB_STRICT) strerr_diefu1sys(111, "tcsetpgrp") ;
        if (!(wgolb & GOLB_SILENT)) strerr_warnwu1sys("tcsetpgrp") ;
      }
    }
  }
  else
  {
    if (setsid() == -1)
    {
      if (wgolb & GOLB_STRICT) strerr_diefu1sys(111, "setsid") ;
      if (!(wgolb & GOLB_SILENT)) strerr_warnwu1sys("setsid") ;
    }
  }

  xexec(argv) ;
}
