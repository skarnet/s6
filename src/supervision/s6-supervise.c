/* ISC license. */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/bytestr.h>
#include <skalibs/uint.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/environ.h>
#include <skalibs/skamisc.h>
#include <s6/ftrigw.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-supervise dir"

typedef enum trans_e trans_t, *trans_t_ref ;
enum trans_e
{
  V_TIMEOUT, V_CHLD, V_TERM, V_HUP, V_QUIT,
  V_a, V_b, V_q, V_h, V_k, V_t, V_i, V_1, V_2, V_f, V_F, V_p, V_c,
  V_o, V_d, V_u, V_x, V_O
} ;

typedef enum state_e state_t, *state_t_ref ;
enum state_e
{
  DOWN,
  UP,
  FINISH,
  LASTUP,
  LASTFINISH
} ;

typedef void action_t (void) ;
typedef action_t *action_t_ref ;

static tain_t deadline ;
static s6_svstatus_t status = { .stamp = TAIN_ZERO, .pid = 0, .flagwant = 1, .flagwantup = 1, .flagpaused = 0, .flagfinishing = 0 } ;
static state_t state = DOWN ;
static int flagsetsid = 1 ;
static int cont = 1 ;

static inline void settimeout (int secs)
{
  tain_addsec_g(&deadline, secs) ;
}

static inline void settimeout_infinite (void)
{
  tain_add_g(&deadline, &tain_infinite_relative) ;
}

static inline void announce (void)
{
  if (!s6_svstatus_write(".", &status))
    strerr_warnwu1sys("write status file") ;
}


/* The action array. */

static void nop (void)
{
}

static void bail (void)
{
  cont = 0 ;
}

static void killa (void)
{
  kill(status.pid, SIGALRM) ;
}

static void killb (void)
{
  kill(status.pid, SIGABRT) ;
}

static void killh (void)
{
  kill(status.pid, SIGHUP) ;
}

static void killq (void)
{
  kill(status.pid, SIGQUIT) ;
}

static void killk (void)
{
  kill(status.pid, SIGKILL) ;
}

static void killt (void)
{
  kill(status.pid, SIGTERM) ;
}

static void killi (void)
{
  kill(status.pid, SIGINT) ;
}

static void kill1 (void)
{
  kill(status.pid, SIGUSR1) ;
}

static void kill2 (void)
{
  kill(status.pid, SIGUSR2) ;
}

static void killp (void)
{
  kill(status.pid, SIGSTOP) ;
  status.flagpaused = 1 ;
  announce() ;
}

static void killc (void)
{
  kill(status.pid, SIGCONT) ;
  status.flagpaused = 0 ;
  announce() ;
}

static void trystart (void)
{
  int p[2] ;
  pid_t pid ;
  if (pipecoe(p) < 0)
  {
    settimeout(60) ;
    strerr_warnwu1sys("pipecoe (waiting 60 seconds)") ;
    return ;
  }
  pid = fork() ;
  if (pid < 0)
  {
    settimeout(60) ;
    strerr_warnwu1sys("fork (waiting 60 seconds)") ;
    fd_close(p[1]) ; fd_close(p[0]) ;
    return ;
  }
  else if (!pid)
  {
    char const *cargv[2] = { "run", 0 } ;
    PROG = "s6-supervise (child)" ;
    selfpipe_finish() ;
    fd_close(p[0]) ;
    if (flagsetsid) setsid() ;
    execve("./run", (char *const *)cargv, (char *const *)environ) ;
    fd_write(p[1], "", 1) ;
    strerr_dieexec(111, "run") ;
  }
  fd_close(p[1]) ;
  {
    char c ;
    switch (fd_read(p[0], &c, 1))
    {
      case -1 :
        fd_close(p[0]) ;
        settimeout(60) ;
        strerr_warnwu1sys("read pipe (waiting 60 seconds)") ;
        kill(pid, SIGKILL) ;
        return ;
      case 1 :
      {
        fd_close(p[0]) ;
        settimeout(10) ;
        strerr_warnwu1x("spawn ./run - waiting 10 seconds") ;
        return ;
      }
    }
  }
  fd_close(p[0]) ;
  settimeout_infinite() ;
  state = UP ;
  status.pid = pid ;
  tain_copynow(&status.stamp) ;
  announce() ;
  ftrigw_notify(S6_SUPERVISE_EVENTDIR, 'u') ;
}

static void downtimeout (void)
{
  if (status.flagwant && status.flagwantup) trystart() ;
  else settimeout_infinite() ;
}

static void down_O (void)
{
  status.flagwant = 0 ;
  announce() ;
}

static void down_o (void)
{
  down_O() ;
  trystart() ;
}

static void down_u (void)
{
  status.flagwant = 1 ;
  status.flagwantup = 1 ;
  announce() ;
  trystart() ;
}

static void down_d (void)
{
  status.flagwant = 1 ;
  status.flagwantup = 0 ;
  announce() ;
}

static void tryfinish (int wstat, int islast)
{
  register pid_t pid = fork() ;
  if (pid < 0)
  {
    strerr_warnwu2sys("fork for ", "./finish") ;
    if (islast) bail() ;
    state = DOWN ;
    status.pid = 0 ;
    settimeout(1) ;
    return ;
  }
  else if (!pid)
  {
    char fmt0[UINT_FMT] ;
    char fmt1[UINT_FMT] ;
    char *cargv[4] = { "finish", fmt0, fmt1, 0 } ;
    selfpipe_finish() ;
    fmt0[uint_fmt(fmt0, WIFSIGNALED(wstat) ? 255 : WEXITSTATUS(wstat))] = 0 ;
    fmt1[uint_fmt(fmt1, WTERMSIG(wstat))] = 0 ;
    if (flagsetsid) setsid() ;
    execve("./finish", cargv, (char *const *)environ) ;
    _exit(111) ;
  }
  status.pid = pid ;
  status.flagfinishing = 1 ;
  state = islast ? LASTFINISH : FINISH ;
  settimeout(5) ;
}

static void uptimeout (void)
{
  settimeout_infinite() ;
  strerr_warnw1x("can't happen: timeout while the service is up!") ;
}

static void up_z (void)
{
  int wstat = status.pid ;
  status.pid = 0 ;
  tain_copynow(&status.stamp) ;
  announce() ;
  ftrigw_notify(S6_SUPERVISE_EVENTDIR, 'd') ;
  tryfinish(wstat, 0) ;
}

static void up_o (void)
{
  status.flagwant = 0 ;
  announce() ;
}

static void up_d (void)
{
  status.flagwant = 1 ;
  status.flagwantup = 0 ;
  killt() ;
  killc() ;
}

static void up_u (void)
{
  status.flagwant = 1 ;
  status.flagwantup = 1 ;
  announce() ;
}

static void closethem (void)
{
  fd_close(0) ;
  fd_close(1) ;
  open_read("/dev/null") ;
  open_write("/dev/null") ;
}

static void up_x (void)
{
  state = LASTUP ;
  closethem() ;
}

static void up_term (void)
{
  up_x() ;
  up_d() ;
}

static void finishtimeout (void)
{
  strerr_warnw1x("finish script takes too long - killing it") ;
  killc() ; killk() ;
  settimeout(3) ;
}

static void finish_z (void)
{
  status.pid = 0 ;
  status.flagfinishing = 0 ;
  state = DOWN ;
  announce() ;
  settimeout(1) ;
}

static void finish_u (void)
{
  status.flagwant = 1 ;
  status.flagwantup = 1 ;
  announce() ;
}

static void finish_x (void)
{
  state = LASTFINISH ;
  closethem() ;
}

static void lastup_z (void)
{
  int wstat = status.pid ;
  status.pid = 0 ;
  tain_copynow(&status.stamp) ;
  announce() ;
  ftrigw_notify(S6_SUPERVISE_EVENTDIR, 'd') ;
  tryfinish(wstat, 1) ;
}

static action_t_ref const actions[5][23] =
{
  { &downtimeout, &nop, &bail, &bail, &bail,
    &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop,
    &down_o, &down_d, &down_u, &bail, &down_O },
  { &uptimeout, &up_z, &up_term, &up_x, &up_term,
    &killa, &killb, &killq, &killh, &killk, &killt, &killi, &kill1, &kill2, &nop, &nop, &killp, &killc,
    &up_o, &up_d, &up_u, &up_x, &up_o },
  { &finishtimeout, &finish_z, &finish_x, &finish_x, &finish_x,
    &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop,
    &up_o, &down_d, &finish_u, &finish_x, &up_o },
  { &uptimeout, &lastup_z, &up_d, &nop, &up_d,
    &killa, &killb, &killq, &killh, &killk, &killt, &killi, &kill1, &kill2, &nop, &nop, &killp, &killc,
    &up_o, &up_d, &nop, &nop, &up_o },
  { &finishtimeout, &bail, &nop, &nop, &nop,
    &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop,
    &nop, &nop, &nop, &nop, &nop }
} ;


/* The main loop.
   It just loops around the iopause(), calling snippets of code in "actions" when needed. */


static void handle_signals (void)
{
  for (;;)
  {
    char c = selfpipe_read() ;
    switch (c)
    {
      case -1 : strerr_diefu1sys(111, "selfpipe_read") ;
      case 0 : return ;
      case SIGCHLD :
        if (!status.pid) wait_reap() ;
        else
        {
          int wstat ;
          int r = wait_pid_nohang(status.pid, &wstat) ;
          if (r < 0)
            if (errno != ECHILD) strerr_diefu1sys(111, "wait_pid_nohang") ;
            else break ;
          else if (!r) break ;
          status.pid = wstat ;
          (*actions[state][V_CHLD])() ;
        }
        break ;
      case SIGTERM :
        (*actions[state][V_TERM])() ;
        break ;
      case SIGHUP :
        (*actions[state][V_HUP])() ;
        break ;
      case SIGQUIT :
        (*actions[state][V_QUIT])() ;
        break ;
      default :
        strerr_dief1x(101, "internal error: inconsistent signal state. Please submit a bug-report.") ;
    }
  }
}

static void handle_control (int fd)
{
  for (;;)
  {
    char c ;
    register int r = sanitize_read(fd_read(fd, &c, 1)) ;
    if (r < 0) strerr_diefu1sys(111, "read " S6_SUPERVISE_CTLDIR "/control") ;
    else if (!r) break ;
    else
    {
      register unsigned int pos = byte_chr("abqhkti12fFpcoduxO", 18, c) ;
      if (pos < 18) (*actions[state][V_a + pos])() ;
    }
  }
}

int main (int argc, char const *const *argv)
{
  iopause_fd x[2] = { { -1, IOPAUSE_READ, 0 }, { -1, IOPAUSE_READ, 0 } } ;
  PROG = "s6-supervise" ;
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  if (chdir(argv[1]) < 0) strerr_diefu2sys(111, "chdir to ", argv[1]) ;
  {
    register unsigned int proglen = str_len(PROG) ;
    register unsigned int namelen = str_len(argv[1]) ;
    char progname[proglen + namelen + 2] ;
    byte_copy(progname, proglen, PROG) ;
    progname[proglen] = ' ' ;
    byte_copy(progname + proglen + 1, namelen + 1, argv[1]) ;
    PROG = progname ;
    if (!fd_sanitize()) strerr_diefu1sys(111, "sanitize stdin and stdout") ;
    x[1].fd = s6_supervise_lock(S6_SUPERVISE_CTLDIR) ;
    if (!ftrigw_fifodir_make(S6_SUPERVISE_EVENTDIR, -1, 0))
      strerr_diefu2sys(111, "mkfifodir ", S6_SUPERVISE_EVENTDIR) ;
    x[0].fd = selfpipe_init() ;
    if (x[0].fd == -1) strerr_diefu1sys(111, "init selfpipe") ;
    if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "ignore SIGPIPE") ;
    {
      sigset_t set ;
      sigemptyset(&set) ;
      sigaddset(&set, SIGTERM) ;
      sigaddset(&set, SIGHUP) ;
      sigaddset(&set, SIGQUIT) ;
      sigaddset(&set, SIGCHLD) ;
      if (selfpipe_trapset(&set) < 0) strerr_diefu1sys(111, "trap signals") ;
    }
    
    if (!ftrigw_clean(S6_SUPERVISE_EVENTDIR))
      strerr_warnwu2sys("ftrigw_clean ", S6_SUPERVISE_EVENTDIR) ;

    {
      struct stat st ;
      if (stat("down", &st) == -1)
      {
        if (errno != ENOENT)
          strerr_diefu1sys(111, "stat down") ;
      }
      else status.flagwantup = 0 ;
      if (stat("nosetsid", &st) == -1)
      {
        if (errno != ENOENT)
          strerr_diefu1sys(111, "stat nosetsid") ;
      }
      else flagsetsid = 0 ;
    }

    tain_now_g() ;
    settimeout(0) ;
    tain_copynow(&status.stamp) ;
    announce() ;
    ftrigw_notify(S6_SUPERVISE_EVENTDIR, 's') ;

    while (cont)
    {
      register int r = iopause_g(x, 2, &deadline) ;
      if (r < 0) strerr_diefu1sys(111, "iopause") ;
      else if (!r) (*actions[state][V_TIMEOUT])() ;
      else
      {
        if ((x[0].revents | x[1].revents) & IOPAUSE_EXCEPT)
          strerr_diefu1x(111, "iopause: trouble with pipes") ;
        if (x[0].revents & IOPAUSE_READ) handle_signals() ;
        else if (x[1].revents & IOPAUSE_READ) handle_control(x[1].fd) ;
      }
    }

    ftrigw_notify(S6_SUPERVISE_EVENTDIR, 'x') ;
  }
  return 0 ;
}
