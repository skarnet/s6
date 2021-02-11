/* ISC license. */

/* For SIGWINCH */
#include <skalibs/nonposix.h>

#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <skalibs/allreadwrite.h>
#include <skalibs/bytestr.h>
#include <skalibs/types.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/skamisc.h>

#include <s6/ftrigw.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-supervise dir"
#define CTL S6_SUPERVISE_CTLDIR "/control"
#define LCK S6_SUPERVISE_CTLDIR "/lock"

#ifdef PATH_MAX
# define S6_PATH_MAX PATH_MAX
#else
# define S6_PATH_MAX 4096
#endif

typedef enum trans_e trans_t, *trans_t_ref ;
enum trans_e
{
  V_TIMEOUT, V_CHLD, V_TERM, V_HUP, V_QUIT, V_INT,
  V_a, V_b, V_q, V_h, V_k, V_t, V_i, V_1, V_2, V_p, V_c, V_y, V_r,
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
static tain_t dontrespawnbefore = TAIN_EPOCH ;
static s6_svstatus_t status = S6_SVSTATUS_ZERO ;
static state_t state = DOWN ;
static int flagdying = 0 ;
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

static int read_file (char const *file, char *buf, size_t n)
{
  ssize_t r = openreadnclose_nb(file, buf, n) ;
  if (r < 0)
  {
    if (errno != ENOENT) strerr_warnwu2sys("open ", file) ;
    return 0 ;
  }
  buf[byte_chr(buf, r, '\n')] = 0 ;
  return 1 ;
}

static int read_uint (char const *file, unsigned int *fd)
{
  char buf[UINT_FMT + 1] ;
  if (!read_file(file, buf, UINT_FMT)) return 0 ;
  if (!uint0_scan(buf, fd))
  {
    strerr_warnw2x("invalid ", file) ;
    return 0 ;
  }
  return 1 ;
}

static inline int read_downsig (void)
{
  int sig = SIGTERM ;
  char buf[16] ;
  if (read_file("down-signal", buf, 15) && !sig0_scan(buf, &sig))
    strerr_warnw1x("invalid down-signal") ;
  return sig ;
}

static void set_down_and_ready (char const *s, unsigned int n)
{
  status.pid = 0 ;
  status.flagfinishing = 0 ;
  status.flagready = 1 ;
  tain_wallclock_read(&status.readystamp) ;
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

static void sigint (void)
{
  pid_t pgid = getpgid(status.pid) ;
  if (pgid == -1) strerr_warnwu1sys("getpgid") ;
  else killpg(pgid, SIGINT) ;
  bail() ;
}

static void closethem (void)
{
  fd_close(0) ;
  fd_close(1) ;
  if (open_readb("/dev/null"))
    strerr_warnwu2sys("open /dev/null for ", "reading") ;
  else if (open_write("/dev/null") != 1 || ndelay_off(1) < 0)
      strerr_warnwu2sys("open /dev/null for ", "writing") ;
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

static void killr (void)
{
  kill(status.pid, read_downsig()) ;
}

static void failcoe (int fd)
{
  int e = errno ;
  fd_write(fd, "", 1) ;
  errno = e ;
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
    sig_restore(SIGPIPE) ;
    if (notifyp[0] >= 0) close(notifyp[0]) ;
    close(p[0]) ;
    if (notifyp[1] >= 0 && fd_move((int)fd, notifyp[1]) < 0)
    {
      failcoe(p[1]) ;
      strerr_diefu1sys(127, "move notification descriptor") ;
    }
    setsid() ;
    execv("./run", (char *const *)cargv) ;
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
  tain_wallclock_read(&status.stamp) ;
  tain_addsec_g(&dontrespawnbefore, 1) ;
  announce() ;
  ftrigw_notifyb_nosig(S6_SUPERVISE_EVENTDIR, "u", 1) ;
}

static void downtimeout (void)
{
  if (status.flagwantup) trystart() ;
  else settimeout_infinite() ;
}

static void down_O (void)
{
  status.flagwantup = 0 ;
  announce() ;
}

static void down_o (void)
{
  down_O() ;
  trystart() ;
}

static void down_u (void)
{
  status.flagwantup = 1 ;
  announce() ;
  trystart() ;
}

static void down_d (void)
{
  status.flagwantup = 0 ;
  announce() ;
}

static int uplastup_z (void)
{
  status.wstat = (int)status.pid ;
  status.flagpaused = 0 ;
  status.flagready = 0 ;
  status.flagthrottled = 0 ;
  flagdying = 0 ;
  tain_wallclock_read(&status.stamp) ;
  if (notifyfd >= 0)
  {
    fd_close(notifyfd) ;
    notifyfd = -1 ;
  }

  {
    unsigned int n ;
    if (!read_uint("max-death-tally", &n)) n = 100 ;
    if (n > S6_MAX_DEATH_TALLY) n = S6_MAX_DEATH_TALLY ;
    if (n)
    {
      s6_dtally_t tab[n+1] ;
      ssize_t m = s6_dtally_read(".", tab, n) ;
      if (m < 0) strerr_warnwu2sys("read ", S6_DTALLY_FILENAME) ;
      else
      {
        tab[m].stamp = status.stamp ;
        tab[m].sig = WIFSIGNALED(status.wstat) ? WTERMSIG(status.wstat) : 0 ;
        tab[m].exitcode = WIFSIGNALED(status.wstat) ? 128 + WTERMSIG(status.wstat) : WEXITSTATUS(status.wstat) ;
        if (!(m >= n ? s6_dtally_write(".", tab+1, n) : s6_dtally_write(".", tab, m+1)))
          strerr_warnwu2sys("write ", S6_DTALLY_FILENAME) ;
      }
    }
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
    sig_restore(SIGPIPE) ;
    fmt0[uint_fmt(fmt0, WIFSIGNALED(status.wstat) ? 256 : WEXITSTATUS(status.wstat))] = 0 ;
    fmt1[uint_fmt(fmt1, WTERMSIG(status.wstat))] = 0 ;
    setsid() ;
    execv("./finish", cargv) ;
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
  if (flagdying)
  {
    killk() ;
    settimeout(5) ;
  }
  else
  {
    settimeout_infinite() ;
    strerr_warnw1x("can't happen: timeout while the service is up!") ;
  }
}

static void up_o (void)
{
  status.flagwantup = 0 ;
  announce() ;
}

static void up_d (void)
{
  tain_t tto ;
  unsigned int timeout ;
  status.flagwantup = 0 ;
  killr() ;
  killc() ;
  if (!read_uint("timeout-kill", &timeout)) timeout = 0 ;
  if (timeout && tain_from_millisecs(&tto, timeout))
  {
    tain_add_g(&deadline, &tto) ;
    flagdying = 1 ;
  }
  else settimeout_infinite() ;
}

static void up_u (void)
{
  status.flagwantup = 1 ;
  announce() ;
}

static void up_x (void)
{
  state = LASTUP ;
  closethem() ;
}

static void up_term (void)
{
  state = LASTUP ;
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
  int wstat = (int)status.pid ;
  if (WIFEXITED(wstat) && WEXITSTATUS(wstat) == 125)
  {
    status.flagwantup = 0 ;
    set_down_and_ready("OD", 2) ;
  }
  else set_down_and_ready("D", 1) ;
}

static void finish_u (void)
{
  status.flagwantup = 1 ;
  announce() ;
}

static void finish_x (void)
{
  state = LASTFINISH ;
  closethem() ;
}

static void lastfinish_z (void)
{
  finish_z() ;
  bail() ;
}

static action_t_ref const actions[5][24] =
{
  { &downtimeout, &nop, &bail, &bail, &bail, &bail,
    &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop,
    &down_o, &down_d, &down_u, &bail, &down_O },
  { &uptimeout, &up_z, &up_term, &up_x, &bail, &sigint,
    &killa, &killb, &killq, &killh, &killk, &killt, &killi, &kill1, &kill2, &killp, &killc, &killy, &killr,
    &up_o, &up_d, &up_u, &up_x, &up_o },
  { &finishtimeout, &finish_z, &finish_x, &finish_x, &bail, &sigint,
    &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop,
    &up_o, &down_d, &finish_u, &finish_x, &up_o },
  { &uptimeout, &lastup_z, &up_d, &closethem, &bail, &sigint,
    &killa, &killb, &killq, &killh, &killk, &killt, &killi, &kill1, &kill2, &killp, &killc, &killy, &killr,
    &up_o, &up_d, &nop, &nop, &up_o },
  { &finishtimeout, &lastfinish_z, &nop, &closethem, &bail, &sigint,
    &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop,
    &nop, &nop, &nop, &nop, &nop }
} ;



/* The main loop.
   It just loops around the iopause(), calling snippets of code in "actions" when needed. */


static inline void handle_notifyfd (void)
{
  char buf[4096] ;
  ssize_t r = 1 ;
  while (r > 0)
  {
    r = sanitize_read(fd_read(notifyfd, buf, 4096)) ;
    if (r > 0 && memchr(buf, '\n', r))
    {
      tain_wallclock_read(&status.readystamp) ;
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
      case SIGINT :
        (*actions[state][V_INT])() ;
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
    ssize_t r = sanitize_read(fd_read(fd, &c, 1)) ;
    if (r < 0) strerr_diefu1sys(111, "read " S6_SUPERVISE_CTLDIR "/control") ;
    else if (!r) break ;
    else
    {
      size_t pos = byte_chr("abqhkti12pcyroduxO", 18, c) ;
      if (pos < 18) (*actions[state][V_a + pos])() ;
    }
  }
}

static int trymkdir (char const *s)
{
  char buf[S6_PATH_MAX] ;
  ssize_t r ;
  if (mkdir(s, 0700) >= 0) return 1 ;
  if (errno != EEXIST) strerr_diefu2sys(111, "mkdir ", s) ;
  r = readlink(s, buf, S6_PATH_MAX) ;
  if (r < 0)
  {
    struct stat st ;
    if (errno != EINVAL)
    {
      errno = EEXIST ;
      strerr_diefu2sys(111, "mkdir ", s) ;
    }
    if (stat(s, &st) < 0)
      strerr_diefu2sys(111, "stat ", s) ;
    if (!S_ISDIR(st.st_mode))
      strerr_dief2x(100, s, " exists and is not a directory") ;
    return 0 ;
  }
  else if (r == S6_PATH_MAX)
  {
    errno = ENAMETOOLONG ;
    strerr_diefu2sys(111, "readlink ", s) ;
  }
  else
  {
    buf[r] = 0 ;
    if (mkdir(buf, 0700) < 0)
      strerr_diefu2sys(111, "mkdir ", buf) ;
    return 1 ;
  }
}

static inline int control_init (void)
{
  mode_t m = umask(0) ;
  int fdctl, fdlck, r ;
  if (trymkdir(S6_SUPERVISE_EVENTDIR))
  {
    if (chown(S6_SUPERVISE_EVENTDIR, -1, getegid()) < 0)
      strerr_diefu1sys(111, "chown " S6_SUPERVISE_EVENTDIR) ;
    if (chmod(S6_SUPERVISE_EVENTDIR, 03730) < 0)
      strerr_diefu1sys(111, "chmod " S6_SUPERVISE_EVENTDIR) ;
  }

  trymkdir(S6_SUPERVISE_CTLDIR) ;
  fdlck = open3(LCK, O_WRONLY | O_NONBLOCK | O_CREAT | O_CLOEXEC, 0644) ;
  if (fdlck < 0) strerr_diefu1sys(111, "open " LCK) ;
  r = fd_lock(fdlck, 1, 1) ;
  if (r < 0) strerr_diefu1sys(111, "lock " LCK) ;
  if (!r) strerr_dief1x(100, "another instance of s6-supervise is already running") ;
 /* fdlck leaks but it's coe */

  if (mkfifo(CTL, 0600) < 0)
  {
    struct stat st ;
    if (errno != EEXIST)
      strerr_diefu1sys(111, "mkfifo " CTL) ;
    if (stat(CTL, &st) < 0)
      strerr_diefu1sys(111, "stat " CTL) ;
    if (!S_ISFIFO(st.st_mode))
      strerr_dief1x(100, CTL " is not a FIFO") ;
  }
  fdctl = openc_read(CTL) ;
  if (fdctl < 0)
    strerr_diefu1sys(111, "open " CTL " for reading") ;
  r = openc_write(CTL) ;
  if (r < 0)
    strerr_diefu1sys(111, "open " CTL " for writing") ;
 /* r leaks but it's coe */

  umask(m) ;
  return fdctl ;
}

int main (int argc, char const *const *argv)
{
  iopause_fd x[3] = { { -1, IOPAUSE_READ, 0 }, { -1, IOPAUSE_READ, 0 }, { -1, IOPAUSE_READ, 0 } } ;
  PROG = "s6-supervise" ;
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  if (chdir(argv[1]) < 0) strerr_diefu2sys(111, "chdir to ", argv[1]) ;
  {
    size_t proglen = strlen(PROG) ;
    size_t namelen = strlen(argv[1]) ;
    char progname[proglen + namelen + 2] ;
    memcpy(progname, PROG, proglen) ;
    progname[proglen] = ' ' ;
    memcpy(progname + proglen + 1, argv[1], namelen + 1) ;
    PROG = progname ;
    if (!fd_sanitize()) strerr_diefu1sys(111, "sanitize stdin and stdout") ;
    x[1].fd = control_init() ;
    x[0].fd = selfpipe_init() ;
    if (x[0].fd == -1) strerr_diefu1sys(111, "init selfpipe") ;
    if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "ignore SIGPIPE") ;
    {
      sigset_t set ;
      sigemptyset(&set) ;
      sigaddset(&set, SIGCHLD) ;
      sigaddset(&set, SIGTERM) ;
      sigaddset(&set, SIGHUP) ;
      sigaddset(&set, SIGQUIT) ;
      sigaddset(&set, SIGINT) ;
      if (selfpipe_trapset(&set) < 0) strerr_diefu1sys(111, "trap signals") ;
    }
    
    if (!ftrigw_clean(S6_SUPERVISE_EVENTDIR))
      strerr_warnwu2sys("ftrigw_clean ", S6_SUPERVISE_EVENTDIR) ;
    {
      int fd = open_trunc(S6_DTALLY_FILENAME) ;
      if (fd < 0) strerr_diefu2sys(111, "truncate ", S6_DTALLY_FILENAME) ;
      fd_close(fd) ;
    }

    if (access("down", F_OK) == 0) status.flagwantup = 0 ;
    else if (errno != ENOENT)
      strerr_diefu1sys(111, "access ./down") ;

    tain_now_set_stopwatch_g() ;
    settimeout(0) ;
    tain_wallclock_read(&status.stamp) ;
    status.readystamp = status.stamp ;
    announce() ;
    ftrigw_notifyb_nosig(S6_SUPERVISE_EVENTDIR, "s", 1) ;

    while (cont)
    {
      int r ;
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
