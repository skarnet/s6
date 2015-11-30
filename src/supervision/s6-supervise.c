/* ISC license. */

/* For SIGWINCH */
#include <skalibs/nonposix.h>

#include <sys/types.h>
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
  V_a, V_b, V_q, V_h, V_k, V_t, V_i, V_1, V_2, V_f, V_F, V_p, V_c, V_y,
  V_o, V_d, V_u, V_x, V_O, V_X
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
static tain_t dontrespawnbefore = TAIN_EPOCH ;
static s6_svstatus_t status = S6_SVSTATUS_ZERO ;
static state_t state = DOWN ;
static int cont = 1 ;
static int notifyfd = -1 ;

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

static int read_uint (char const *file, unsigned int *fd)
{
  char buf[UINT_FMT + 1] ;
  register int r = openreadnclose_nb(file, buf, UINT_FMT) ;
  if (r < 0)
  {
    if (errno != ENOENT) strerr_warnwu2sys("open ", file) ;
    return 0 ;
  }
  buf[byte_chr(buf, r, '\n')] = 0 ;
  if (!uint0_scan(buf, fd))
  {
    strerr_warnw2x("invalid ", file) ;
    return 0 ;
  }
  return 1 ;
}

static void set_down_and_ready (char const *s, unsigned int n)
{
  status.pid = 0 ;
  status.flagfinishing = 0 ;
  status.flagready = 1 ;
  tain_copynow(&status.readystamp) ;
  state = DOWN ;
  if (tain_future(&dontrespawnbefore)) deadline = dontrespawnbefore ;
  else tain_copynow(&deadline) ;
  announce() ;
  ftrigw_notifyb_nosig(S6_SUPERVISE_EVENTDIR, s, n) ;
}


/* The action array. */

static void nop (void)
{
}

static void bail (void)
{
  cont = 0 ;
}

static void closethem (void)
{
  fd_close(0) ;
  fd_close(1) ;
  fd_close(2) ;
  open_readb("/dev/null") ;
  open_write("/dev/null") ; ndelay_off(1) ;
  open_write("/dev/null") ; ndelay_off(2) ;
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

static void killy (void)
{
  kill(status.pid, SIGWINCH) ;
}

static void failcoe (int fd)
{
  register int e = errno ;
  fd_write(fd, "", 1) ;
  errno = e ;
}

static int maybesetsid (void)
{
  if (access("nosetsid", F_OK) < 0)
  {
    if (errno != ENOENT) return 0 ;
    setsid() ;
  }
  return 1 ;
}

static void trystart (void)
{
  int p[2] ;
  int notifyp[2] = { -1, -1 } ;
  unsigned int fd ;
  pid_t pid ;
  if (pipecoe(p) < 0)
  {
    settimeout(60) ;
    strerr_warnwu1sys("pipe (waiting 60 seconds)") ;
    return ;
  }
  if (read_uint("notification-fd", &fd) && pipe(notifyp) < 0)
  {
    settimeout(60) ;
    strerr_warnwu1sys("pipe (waiting 60 seconds)") ;
    fd_close(p[1]) ; fd_close(p[0]) ;
    return ;
  }
  pid = fork() ;
  if (pid < 0)
  {
    settimeout(60) ;
    strerr_warnwu1sys("fork (waiting 60 seconds)") ;
    if (notifyp[1] >= 0) fd_close(notifyp[1]) ;
    if (notifyp[0] >= 0) fd_close(notifyp[0]) ;
    fd_close(p[1]) ; fd_close(p[0]) ;
    return ;
  }
  else if (!pid)
  {
    char const *cargv[2] = { "run", 0 } ;
    PROG = "s6-supervise (child)" ;
    selfpipe_finish() ;
    if (notifyp[0] >= 0) close(notifyp[0]) ;
    close(p[0]) ;
    if (notifyp[1] >= 0 && fd_move((int)fd, notifyp[1]) < 0)
    {
      failcoe(p[1]) ;
      strerr_diefu1sys(127, "move notification descriptor") ;
    }
    if (!maybesetsid())
    {
      failcoe(p[1]) ;
      strerr_diefu1sys(127, "access ./nosetsid") ;
    }
    execve("./run", (char *const *)cargv, (char *const *)environ) ;
    failcoe(p[1]) ;
    strerr_dieexec(127, "run") ;
  }
  if (notifyp[1] >= 0) fd_close(notifyp[1]) ;
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
  notifyfd = notifyp[0] ;
  settimeout_infinite() ;
  state = UP ;
  status.pid = pid ;
  status.flagready = 0 ;
  tain_copynow(&status.stamp) ;
  tain_addsec_g(&dontrespawnbefore, 1) ;
  announce() ;
  ftrigw_notifyb_nosig(S6_SUPERVISE_EVENTDIR, "u", 1) ;
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

static int uplastup_z (void)
{
  status.wstat = (int)status.pid ;
  status.flagpaused = 0 ;
  status.flagready = 0 ;
  tain_copynow(&status.stamp) ;
  if (notifyfd >= 0)
  {
    fd_close(notifyfd) ;
    notifyfd = -1 ;
  }
  status.pid = fork() ;
  if (status.pid < 0)
  {
    strerr_warnwu2sys("fork for ", "./finish") ;
    set_down_and_ready("dD", 2) ;
    return 0 ;
  }
  else if (!status.pid)
  {
    char fmt0[UINT_FMT] ;
    char fmt1[UINT_FMT] ;
    char *cargv[4] = { "finish", fmt0, fmt1, 0 } ;
    selfpipe_finish() ;
    fmt0[uint_fmt(fmt0, WIFSIGNALED(status.wstat) ? 256 : WEXITSTATUS(status.wstat))] = 0 ;
    fmt1[uint_fmt(fmt1, WTERMSIG(status.wstat))] = 0 ;
    maybesetsid() ;
    execve("./finish", cargv, (char *const *)environ) ;
    _exit(127) ;
  }
  status.flagfinishing = 1 ;
  announce() ;
  ftrigw_notifyb_nosig(S6_SUPERVISE_EVENTDIR, "d", 1) ;
  {
    tain_t tto ;
    unsigned int timeout ;
    if (!read_uint("timeout-finish", &timeout)) timeout = 5000 ;
    if (timeout && tain_from_millisecs(&tto, timeout))
      tain_add_g(&deadline, &tto) ;
    else settimeout_infinite() ;
  }
  return 1 ;
}

static void up_z (void)
{
  if (uplastup_z()) state = FINISH ;
}

static void lastup_z (void)
{
  if (uplastup_z()) state = LASTFINISH ;
  else bail() ;
}

static void uptimeout (void)
{
  settimeout_infinite() ;
  strerr_warnw1x("can't happen: timeout while the service is up!") ;
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

static void up_x (void)
{
  state = LASTUP ;
}

static void up_X (void)
{
  closethem() ;
  up_x() ;
}

static void up_term (void)
{
  up_x() ;
  up_d() ;
}

static void finishtimeout (void)
{
  strerr_warnw1x("finish script lifetime reached maximum value - sending it a SIGKILL") ;
  killc() ; killk() ;
  settimeout(5) ;
}

static void finish_z (void)
{
  set_down_and_ready("D", 1) ;
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
}

static void finish_X (void)
{
  closethem() ;
  finish_x() ;
}

static action_t_ref const actions[5][25] =
{
  { &downtimeout, &nop, &bail, &bail, &bail,
    &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop,
    &down_o, &down_d, &down_u, &bail, &down_O, &bail },
  { &uptimeout, &up_z, &up_term, &up_x, &up_X,
    &killa, &killb, &killq, &killh, &killk, &killt, &killi, &kill1, &kill2, &nop, &nop, &killp, &killc, &killy,
    &up_o, &up_d, &up_u, &up_x, &up_o, &up_X },
  { &finishtimeout, &finish_z, &finish_x, &finish_x, &finish_X,
    &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop,
    &up_o, &down_d, &finish_u, &finish_x, &up_o, &finish_X },
  { &uptimeout, &lastup_z, &up_d, &nop, &closethem,
    &killa, &killb, &killq, &killh, &killk, &killt, &killi, &kill1, &kill2, &nop, &nop, &killp, &killc, &killy,
    &up_o, &up_d, &nop, &nop, &up_o, &closethem },
  { &finishtimeout, &bail, &nop, &nop, &closethem,
    &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop,
    &nop, &nop, &nop, &nop, &nop, &closethem }
} ;



/* The main loop.
   It just loops around the iopause(), calling snippets of code in "actions" when needed. */


static inline void handle_notifyfd (void)
{
  char buf[4096] ;
  register int r = 1 ;
  while (r > 0)
  {
    r = sanitize_read(fd_read(notifyfd, buf, 4096)) ;
    if (r > 0 && byte_chr(buf, r, '\n') < r)
    {
      tain_copynow(&status.readystamp) ;
      status.flagready = 1 ;
      announce() ;
      ftrigw_notifyb_nosig(S6_SUPERVISE_EVENTDIR, "U", 1) ;
      r = -1 ;
    }
    if (r < 0)
    {
      fd_close(notifyfd) ;
      notifyfd = -1 ;
    }
  }
}

static inline void handle_signals (void)
{
  for (;;)
  {
    switch (selfpipe_read())
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
          status.pid = (pid_t)wstat ; /* don't overwrite status.wstat if it's ./finish */
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

static inline void handle_control (int fd)
{
  for (;;)
  {
    char c ;
    register int r = sanitize_read(fd_read(fd, &c, 1)) ;
    if (r < 0) strerr_diefu1sys(111, "read " S6_SUPERVISE_CTLDIR "/control") ;
    else if (!r) break ;
    else
    {
      register unsigned int pos = byte_chr("abqhkti12fFpcyoduxOX", 20, c) ;
      if (pos < 20) (*actions[state][V_a + pos])() ;
    }
  }
}

int main (int argc, char const *const *argv)
{
  iopause_fd x[3] = { { -1, IOPAUSE_READ, 0 }, { -1, IOPAUSE_READ, 0 }, { -1, IOPAUSE_READ, 0 } } ;
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
    if (!ftrigw_fifodir_make(S6_SUPERVISE_EVENTDIR, getegid(), 0))
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

    if (access("down", F_OK) == 0) status.flagwantup = 0 ;
    else if (errno != ENOENT)
      strerr_diefu1sys(111, "access ./down") ;

    tain_now_g() ;
    settimeout(0) ;
    tain_copynow(&status.stamp) ;
    status.readystamp = status.stamp ;
    announce() ;
    ftrigw_notifyb_nosig(S6_SUPERVISE_EVENTDIR, "s", 1) ;

    while (cont)
    {
      register int r ;
      x[2].fd = notifyfd ;
      r = iopause_g(x, 2 + (notifyfd >= 0), &deadline) ;
      if (r < 0) strerr_diefu1sys(111, "iopause") ;
      else if (!r) (*actions[state][V_TIMEOUT])() ;
      else
      {
        if ((x[0].revents | x[1].revents) & IOPAUSE_EXCEPT)
          strerr_diefu1x(111, "iopause: trouble with pipes") ;
        if (notifyfd >= 0 && x[2].revents & IOPAUSE_READ) handle_notifyfd() ;
        if (x[0].revents & IOPAUSE_READ) handle_signals() ;
        else if (x[1].revents & IOPAUSE_READ) handle_control(x[1].fd) ;
      }
    }

    ftrigw_notifyb_nosig(S6_SUPERVISE_EVENTDIR, "x", 1) ;
  }
  return 0 ;
}
