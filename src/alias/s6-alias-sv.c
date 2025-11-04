 /* ISC license. */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <skalibs/uint32.h>
#include <skalibs/sgetopt.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr.h>
#include <skalibs/cspawn.h>
#include <skalibs/djbunix.h>

#include <s6/config.h>
#include <s6/supervise.h>

#define USAGE "s6-alias-sv [ -v ] [ -w sec ] command services..."
#define dieusage() strerr_dieusage(100, USAGE)
#define dienomem() strerr_diefu1sys(111, "stralloc_catb")

typedef int exec_func (char const *, char const *const *) ;
typedef exec_func *exec_func_ref ;

typedef struct info_s info_t, *info_t_ref ;
struct info_s
{
  char const *name ;
  exec_func_ref f ;
} ;

static int dowait = 0 ;
static uint32_t secs = 7 ;

static void warnnolog (void)
{
  strerr_warnw1x("s6-svc only sends commands to a single service, even if it has a dedicated logger") ;
}

static void warnnokill (void)
{
  strerr_warnw1x("s6-supervise pilots a kill signal via the timeout-kill file in the service directory") ;
}

static int info_cmp (void const *a, void const *b)
{
  char const *name = a ;
  info_t const *info = b ;
  return strcmp(name, info->name) ;
}

static int spawnit (char const *const *argv, char const *const *envp)
{
  int wstat ;
  pid_t r ;
  pid_t pid = cspawn(argv[0], argv, envp, 0, 0, 0) ;
  if (!pid)
  {
    strerr_warnwu2sys("spawn ", argv[0]) ;
    return 1 ;
  }
  r = wait_pid(pid, &wstat) ;
  if (r != pid)
  {
    strerr_warnwu2sys("wait for ", argv[0]) ;
    return 1 ;
  }
  return !!WIFSIGNALED(wstat) || !!WEXITSTATUS(wstat) ;
}

static int simple_svc (char const *dir, char const *options, char const *const *envp)
{
  char const *argv[5] = { S6_BINPREFIX "s6-svc", options, "--", dir, 0 } ;
  return spawnit(argv, envp) ;
}

static int complex_svc (char const *dir, char const *order, char waitfor, char const *const *envp)
{
  char warg[4] = "-w?" ;
  char fmt[2 + UINT32_FMT] = "-T" ;
  char const *argv[7] = { S6_BINPREFIX "s6-svc", warg, fmt, order, "--", dir, 0 } ;
  fmt[2 + uint32_fmt(fmt + 2, 1000 * secs)] = 0 ;
  warg[2] = waitfor ;
  return spawnit(argv, envp) ;
}

static int status (char const *dir, char const *const *envp)
{
  int e ;
  char const *argv[4] = { S6_BINPREFIX "s6-svstat", "--", dir, 0 } ;
  size_t dirlen = strlen(dir) ;
  buffer_puts(buffer_1, dir) ;
  buffer_putsflush(buffer_1, ": ") ;
  e = spawnit(argv, envp) ;
  if (dirlen < 5 || strcmp(dir + dirlen - 4, "/log"))
  {
    struct stat st ;
    char log[dirlen + 5] ;
    memcpy(log, dir, dirlen) ;
    memcpy(log + dirlen, "/log", 5) ;
    if (stat(log, &st) < 0)
    {
      if (errno != ENOENT)
      {
        strerr_warnwu2sys("stat", log) ;
        e = 1 ;
      }
    }
    else if (S_ISDIR(st.st_mode))
    {
      argv[2] = log ;
      buffer_puts(buffer_1, log) ;
      buffer_putsflush(buffer_1, ": ") ;
      e |= spawnit(argv, envp) ;
    }
  }
  return e ;
}

static int action (char const *dir, char const *simple, char const *cplx, char waitchar, char const *const *envp)
{
  if (dowait)
  {
    int e = complex_svc(dir, cplx, waitchar, envp) ;
    return e | status(dir, envp) ;
  }
  else return simple_svc(dir, simple, envp) ;
}

static int usr1_h (char const *dir, char const *const *envp)
{
  return simple_svc(dir, "-1", envp) ;
}

static int usr2_h (char const *dir, char const *const *envp)
{
  return simple_svc(dir, "-2", envp) ;
}

static int alarm_h (char const *dir, char const *const *envp)
{
  return simple_svc(dir, "-a", envp) ;
}

static int cont_h (char const *dir, char const *const *envp)
{
  return action(dir, "-c", "-o", 'U', envp) ;
}

static int down (char const *dir, char const *const *envp)
{
  return action(dir, "-d", "-d", 'D', envp) ;
}

static int forcedown (char const *dir, char const *const *envp)
{
  warnnokill() ;
  return down(dir, envp) ;
}

static int bail (char const *dir, char const *const *envp)
{
  int e ;
  warnnolog() ;
  e = action(dir, "-xd", "-d", 'D', envp) ;
  if (dowait) e |= simple_svc(dir, "-x", envp) ;
  return e ;
}

static int forcebail (char const *dir, char const *const *envp)
{
  warnnokill() ;
  return bail(dir, envp) ;
}

static int hup_h (char const *dir, char const *const *envp)
{
  return simple_svc(dir, "-h", envp) ;
}

static int int_h (char const *dir, char const *const *envp)
{
  return simple_svc(dir, "-i", envp) ;
}

static int kill_h (char const *dir, char const *const *envp)
{
  return simple_svc(dir, "-k", envp) ;
}

static int once (char const *dir, char const *const *envp)
{
  return action(dir, "-o", "-o", 'U', envp) ;
}

static int pause_h (char const *dir, char const *const *envp)
{
  return simple_svc(dir, "-p", envp) ;
}

static int quit_h (char const *dir, char const *const *envp)
{
  return simple_svc(dir, "-q", envp) ;
}

static int term_h (char const *dir, char const *const *envp)
{
  return action(dir, "-t", "-r", 'R', envp) ;
}

static int up (char const *dir, char const *const *envp)
{
  return action(dir, "-u", "-u", 'U', envp) ;
}

static int check (char const *dir, char const *const *envp)
{
  int e ;
  s6_svstatus_t svst ;
  char warg[3] = "-?" ;
  char fmt[2 + UINT32_FMT] = "-t" ;
  char const *argv[6] = { S6_BINPREFIX "s6-svwait", warg, fmt, "--", dir, 0 } ;
  fmt[2 + uint32_fmt(fmt + 2, 1000 * secs)] = 0 ;
  if (!s6_svstatus_read(dir, &svst)) return 1 ;
  warg[1] = svst.flagwantup ? 'U' : 'D' ;
  e = spawnit(argv, envp) ;
  return e | status(dir, envp) ;
}

static int lsb_reload (char const *dir, char const *const *envp)
{
  hup_h(dir, envp) ;
  return status(dir, envp) ;
}

static int lsb_restart (char const *dir, char const *const *envp)
{
  int e = complex_svc(dir, "-ru", 'U', envp) ;
  return e | status(dir, envp) ;
}

static int lsb_start (char const *dir, char const *const *envp)
{
  int e = complex_svc(dir, "-u", 'U', envp) ;
  return e | status(dir, envp) ;
}

static int lsb_stop (char const *dir, char const *const *envp)
{
  int e = complex_svc(dir, "-d", 'D', envp) ;
  return e | status(dir, envp) ;
}

static int lsb_shutdown (char const *dir, char const *const *envp)
{
  int e ;
  warnnolog() ;
  e = complex_svc(dir, "-d", 'D', envp) ;
  e |= status(dir, envp) ;
  return e | simple_svc(dir, "-x", envp) ;
}

static int lsb_forcereload (char const *dir, char const *const *envp)
{
  warnnokill() ;
  return lsb_reload(dir, envp) ;
}

static int lsb_forcerestart (char const *dir, char const *const *envp)
{
  warnnokill() ;
  return lsb_restart(dir, envp) ;
}

static int lsb_forcestop (char const *dir, char const *const *envp)
{
  warnnokill() ;
  return lsb_stop(dir, envp) ;
}

static int lsb_forceshutdown (char const *dir, char const *const *envp)
{
  warnnokill() ;
  return lsb_shutdown(dir, envp) ;
}

static int lsb_tryrestart (char const *dir, char const *const *envp)
{
  int e = 0 ;
  s6_svstatus_t svst ;
  if (s6_svstatus_read(dir, &svst) && svst.flagwantup && svst.pid && !svst.flagfinishing)
    e = complex_svc(dir, "-r", 'U', envp) ;    
  return e | status(dir, envp) ;
}

static int lsb_forcetryrestart (char const *dir, char const *const *envp)
{
  warnnokill() ;
  return lsb_tryrestart(dir, envp) ;
}

static info_t const commands[] =
{
  { .name = "1", .f = &usr1_h },
  { .name = "2", .f = &usr2_h },
  { .name = "D", .f = &forcedown },
  { .name = "E", .f = &forcebail },
  { .name = "T", .f = &lsb_forcetryrestart },
  { .name = "X", .f = &forcebail },
  { .name = "a", .f = &alarm_h },
  { .name = "al", .f = &alarm_h },
  { .name = "ala", .f = &alarm_h },
  { .name = "alar", .f = &alarm_h },
  { .name = "alarm", .f = &alarm_h },
  { .name = "c", .f = &cont_h },
  { .name = "check", .f = &check },
  { .name = "co", .f = &cont_h },
  { .name = "con", .f = &cont_h },
  { .name = "cont", .f = &cont_h },
  { .name = "d", .f = &down },
  { .name = "do", .f = &down },
  { .name = "dow", .f = &down },
  { .name = "down", .f = &down },
  { .name = "e", .f = &bail },
  { .name = "ex", .f = &bail },
  { .name = "exi", .f = &bail },
  { .name = "exit", .f = &bail },
  { .name = "force-reload", .f = &lsb_forcereload },
  { .name = "force-restart", .f = &lsb_forcerestart },
  { .name = "force-shutdown", .f = &lsb_forceshutdown },
  { .name = "force-stop", .f = &lsb_forcestop },
  { .name = "h", .f = &hup_h },
  { .name = "hu", .f = &hup_h },
  { .name = "hup", .f = &hup_h },
  { .name = "i", .f = &int_h },
  { .name = "in", .f = &int_h },
  { .name = "int", .f = &int_h },
  { .name = "inte", .f = &int_h },
  { .name = "inter", .f = &int_h },
  { .name = "interr", .f = &int_h },
  { .name = "interru", .f = &int_h },
  { .name = "interrup", .f = &int_h },
  { .name = "interrupt", .f = &int_h },
  { .name = "k", .f = &kill_h },
  { .name = "ki", .f = &kill_h },
  { .name = "kil", .f = &kill_h },
  { .name = "kill", .f = &kill_h },
  { .name = "o", .f = &once },
  { .name = "on", .f = &once },
  { .name = "onc", .f = &once },
  { .name = "once", .f = &once },
  { .name = "p", .f = &pause_h },
  { .name = "pa", .f = &pause_h },
  { .name = "pau", .f = &pause_h },
  { .name = "paus", .f = &pause_h },
  { .name = "pause", .f = &pause_h },
  { .name = "q", .f = &quit_h },
  { .name = "qu", .f = &quit_h },
  { .name = "qui", .f = &quit_h },
  { .name = "quit", .f = &quit_h },
  { .name = "reload", .f = &lsb_reload },
  { .name = "restart", .f = &lsb_restart },
  { .name = "s", .f = &status },
  { .name = "shutdown", .f = &lsb_shutdown },
  { .name = "st", .f = &status },
  { .name = "sta", .f = &status },
  { .name = "start", .f = &lsb_start },
  { .name = "stat", .f = &status },
  { .name = "statu", .f = &status },
  { .name = "status", .f = &status },
  { .name = "stop", .f = &lsb_stop },
  { .name = "t", .f = &term_h },
  { .name = "te", .f = &term_h },
  { .name = "ter", .f = &term_h },
  { .name = "term", .f = &term_h },
  { .name = "try-restart", .f = &lsb_tryrestart },
  { .name = "u", .f = &up },
  { .name = "up", .f = &up },
  { .name = "x", .f = &bail }
} ;

int main (int argc, char const *const *argv, char const *const *envp)
{
  int e = 0 ;
  info_t *p ;
  char const *x = getenv("SVWAIT") ;
  char const *scandir = getenv("SVDIR") ;
  size_t scandirlen ;
  PROG = "s6-alias-sv" ;
  if (!scandir) scandir = "/run/service" ; /* TODO: read from s6li config if any? */
  scandirlen = strlen(scandir) ;
  if (x)
  {
    if (!uint320_scan(x, &secs))
      strerr_warnw1x("invalid SVWAIT value") ;
  }

  {
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "vw:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'v' : dowait = 1 ; break ;
        case 'w' : if (!uint320_scan(l.arg, &secs)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }

  if (argc < 2) dieusage() ;
  p = bsearch(argv[0], commands, sizeof(commands) / sizeof(info_t), sizeof(info_t), &info_cmp) ;
  if (!p) strerr_dief2x(100, "unknown command: ", argv[0]) ;

  for (argv++ ; *argv ; argv++)
  {
    if (!argv[0][0]) continue ;
    if (argv[0][0] == '/' || argv[0][strlen(argv[0]) - 1] == '/' || (argv[0][0] == '.' && (!argv[0][1] || argv[0][1] == '/' || (argv[0][1] == '.' && (!argv[0][2] || argv[0][2] == '/')))))
      e += (*p->f)(*argv, envp) ;
    else
    {
      int what = 1 ;
      struct stat st ;
      size_t len = strlen(*argv) ;
      char fn[scandirlen + len + 2] ;
      memcpy(fn, scandir, scandirlen) ;
      fn[scandirlen] = '/' ;
      memcpy(fn + scandirlen + 1, *argv, len + 1) ;
      if (stat(fn, &st) < 0)  /* XXX: TOCTOU but we don't care */
      {
        if (errno != ENOENT)
        {
          e++ ;
          what = 0 ;
          strerr_warnwu2sys("stat ", fn) ;
        }
      }
      else if (S_ISDIR(st.st_mode)) what = 2 ;
      if (what) e += (*p->f)(what > 1 ? fn : *argv, envp) ;
    }
  }
  return e ;
}
