 /* ISC license. */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <skalibs/buffer.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/skamisc.h>
#include <skalibs/exec.h>

#include <s6/config.h>

#define USAGE "s6-alias [ -v ] cmdname options..."
#define dienomem() strerr_diefu1sys(111, "stralloc_catb")
#define dieusage() strerr_dieusage(100, USAGE)

static unsigned int verbosity = 0 ;

typedef void exec_func (int, char const *const *) ;
typedef exec_func *exec_func_ref ;

typedef struct info_s info_t, *info_t_ref ;
struct info_s
{
  char const *name ;
  char const *cmd ;
  exec_func_ref f ;
} ;

static int info_cmp (void const *a, void const *b)
{
  char const *name = a ;
  info_t const *info = b ;
  return strcmp(name, info->name) ;
}

#if defined(S6_WRAP_DAEMONTOOLS) || defined (S6_WRAP_RUNIT)
static void noboot (char const *name)
{
  strerr_dief3x(100, "s6 does not provide a ", name, " emulation. To boot on a s6 supervision tree, please consider the s6-linux-init package.") ;
}
#endif

#ifdef S6_WRAP_DAEMONTOOLS

static void readproctitle (int argc, char const *const *argv)
{
  (void)argc ;
  (void)argv ;
  noboot("readproctitle") ;
}

static void svscanboot (int argc, char const *const *argv)
{
  (void)argc ;
  (void)argv ;
  noboot("svscanboot") ;
}

#endif

#ifdef S6_WRAP_RUNIT

static void runit (int argc, char const *const *argv)
{
  (void)argc ;
  (void)argv ;
  noboot("runit") ;
}

static void runit_init (int argc, char const *const *argv)
{
  (void)argc ;
  (void)argv ;
  noboot("runit-init") ;
}

static void runsvchdir (int argc, char const *const *argv)
{
  (void)argc ;
  (void)argv ;
  strerr_dief1x(100, "s6 does not provide a runsvchdir emulation. To handle several different sets of services, please consider the s6-rc package.") ;
}

static void runsvdir (int argc, char const *const *argv)
{
  char const *newargv[4] = { S6_BINPREFIX "s6-svscan", "-t0", 0, 0 } ;
  int dosetsid = 0 ;
  subgetopt l = SUBGETOPT_ZERO ;
  for (;;)
  {
    int opt = subgetopt_r(argc, argv, "P", &l) ;
    if (opt == -1) break ;
    switch (opt)
    {
      case 'P' : dosetsid = 1 ; break ;
      default : dieusage() ;
    }
  }
  argc -= l.ind ; argv += l.ind ;
  if (dosetsid)
    strerr_warnw1x("-P option ignored: s6-svscan does not run its supervisor processes (s6-supervise) in a new session. However, by default, it runs every service in a new session.") ;
  if (argc >= 2)
    strerr_warnw1x("s6-svscan does not support logging to a readproctitle process. To log the output of your supervision tree, please consider using the s6-linux-init package.") ;
  newargv[2] = argv[0] ;
  if (verbosity)
  {
    buffer_puts(buffer_2, PROG) ;
    buffer_puts(buffer_2, ": info: executing command line:") ;
    for (char const *const *p = argv ; *p ; p++)
    {
      buffer_puts(buffer_2, " ") ;
      buffer_puts(buffer_2, *p) ;
    }
    buffer_putsflush(buffer_2, "\n") ;
  }
  xexec(newargv) ;
}

static void svlogd (int argc, char const *const *argv)
{
  (void)argc ;
  (void)argv ;
  strerr_dief1x(100, "the s6-log program is similar to svlogd, but uses a different filtering syntax and does not use a config file in the logdir. Please see https://skarnet.org/software/s6/s6-log.html") ;
}

static void utmpset (int argc, char const *const *argv)
{
  (void)argc ;
  (void)argv ;
  strerr_dief1x(100, "s6 does not provide a utmpset emulation. To handle utmp records, please consider the s6-linux-init package, along with the utmps package if necessary.") ;
}

#endif

static info_t const aliases[] =
{
#ifdef S6_WRAP_RUNIT
  { .name = "chpst", .cmd = S6_BINPREFIX "s6-alias-chpst", .f = 0 },
#endif
#ifdef S6_WRAP_DAEMONTOOLS
  { .name = "envdir", .cmd = S6_BINPREFIX "s6-envdir", .f = 0 },
  { .name = "envuidgid", .cmd = S6_BINPREFIX "s6-envuidgid", .f = 0 },
  { .name = "fghack", .cmd = S6_BINPREFIX "s6-fghack", .f = 0 },
  { .name = "multilog", .cmd = S6_BINPREFIX "s6-log", .f = 0 },
  { .name = "pgrphack", .cmd = S6_BINPREFIX "s6-setsid", .f = 0 },
  { .name = "readproctitle", .cmd = 0, .f = &readproctitle },
#endif
#ifdef S6_WRAP_RUNIT
  { .name = "runit", .cmd = 0, .f = &runit },
  { .name = "runit-init", .cmd = 0, .f = &runit_init },
  { .name = "runsv", .cmd = S6_BINPREFIX "s6-supervise", .f = 0 },
  { .name = "runsvchdir", .cmd = 0, .f = &runsvchdir },
  { .name = "runsvdir", .cmd = 0, .f = &runsvdir },
#endif
#ifdef S6_WRAP_DAEMONTOOLS
  { .name = "setlock", .cmd = S6_BINPREFIX "s6-setlock", .f = 0 },
  { .name = "setuidgid", .cmd = S6_BINPREFIX "s6-setuidgid", .f = 0 },
  { .name = "softlimit", .cmd = S6_BINPREFIX "s6-softlimit", .f = 0 },
  { .name = "supervise", .cmd = S6_BINPREFIX "s6-supervise", .f = 0 },
#endif
#ifdef S6_WRAP_RUNIT
  { .name = "sv", .cmd = S6_BINPREFIX "s6-alias-sv", .f = 0 },
#endif
#ifdef S6_WRAP_DAEMONTOOLS
  { .name = "svc", .cmd = S6_BINPREFIX "s6-svc", .f = 0 },
#endif
#ifdef S6_WRAP_RUNIT
  { .name = "svlogd", .cmd = 0, .f = &svlogd },
#endif
#ifdef S6_WRAP_DAEMONTOOLS
  { .name = "svok", .cmd = S6_BINPREFIX "s6-svok", .f = 0 },
  { .name = "svscan", .cmd = S6_BINPREFIX "s6-svscan", .f = 0 },
  { .name = "svscanboot", .cmd = 0, .f = &svscanboot },
  { .name = "svstat", .cmd = S6_BINPREFIX "s6-svstat", .f = 0 },
  { .name = "tai64n", .cmd = S6_BINPREFIX "s6-tai64n", .f = 0 },
  { .name = "tai64nlocal", .cmd = S6_BINPREFIX "s6-tai64nlocal", .f = 0 },
#endif
#ifdef S6_WRAP_RUNIT
  { .name = "utmpset", .cmd = 0, .f = &utmpset },
#endif
} ;

int main (int argc, char const **argv)
{
  char const *name = argv[0] ;
  stralloc sa = STRALLOC_ZERO ;
  info_t *p ;
  PROG = "s6-alias" ;

  if (!sabasename(&sa, name, strlen(name)) || !stralloc_0(&sa)) dienomem() ;
  if (!strcmp(sa.s, PROG))
  {
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "v", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'v' : verbosity++ ; break ;
        default : strerr_dieusage(100, USAGE) ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (!argc) dieusage() ;
    name = *argv ;
    stralloc_free(&sa) ;
  }
  else name = sa.s ;

  p = bsearch(name, aliases, sizeof(aliases) / sizeof(info_t), sizeof(info_t), &info_cmp) ;
  if (!p) strerr_dief2x(100, "unknown alias: ", name) ;
  if (p->cmd)
  {
    argv[0] = p->cmd ;
    if (verbosity)
      strerr_warni4x("the s6 version of ", name, " is ", p->cmd) ;
    xexec(argv) ;
  }
  else (*p->f)(argc, argv) ;
  strerr_dief1x(101, "can't happen: incorrect alias handler. Please submit a bug-report.") ;
}
