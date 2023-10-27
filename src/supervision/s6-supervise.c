/* ISC license. */

/* For SIGWINCH */
#include <skalibs/nonposix.h>

#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <skalibs/posixplz.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/bytestr.h>
#include <skalibs/types.h>
#include <skalibs/strerr.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/cspawn.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/skamisc.h>

#include <s6/config.h>
#include <s6/ftrigw.h>
#include <s6/supervise.h>

#define USAGE "s6-supervise dir"
#define CTL S6_SUPERVISE_CTLDIR "/control"
#define LCK S6_SUPERVISE_CTLDIR "/lock"
#define SLCK S6_SUPERVISE_CTLDIR "/service-lock"

#define S6_PATH_MAX 512

typedef enum trans_e trans_t, *trans_t_ref ;
enum trans_e
{
  V_TIMEOUT, V_CHLD, V_TERM, V_HUP, V_QUIT, V_INT,
  V_a, V_b, V_q, V_h, V_k, V_t, V_i, V_1, V_2, V_p, V_c, V_y, V_r,
  V_o, V_d, V_u, V_D, V_U, V_x, V_O, V_Q
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

struct gflags_s
{
  uint8_t cont : 1 ;
  uint8_t dying : 1 ;
} gflags =
{
  .cont = 1,
  .dying = 0
} ;

typedef void action_t (void) ;
typedef action_t *action_t_ref ;

static tain deadline ;
static tain nextstart = TAIN_ZERO ;
static s6_svstatus_t status = S6_SVSTATUS_ZERO ;
static state_t state = DOWN ;
static int notifyfd = -1 ;
static char const *servicename = 0 ;
static rlim_t maxfd ;

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
  if (r == -1)
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
  state = DOWN ;
  if (tai_sec(tain_secp(&nextstart))) deadline = nextstart ;
  else tain_addsec_g(&deadline, 1) ;
  tain_wallclock_read(&status.readystamp) ;
  announce() ;
  ftrigw_notifyb_nosig(S6_SUPERVISE_EVENTDIR, s, n) ;
}


/* The action array. */

static void nop (void)
{
}

static void bail (void)
{
  gflags.cont = 0 ;
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

static void adddown (void)
{
  if (!openwritenclose_unsafe("down", "", 0))
    strerr_warnwu2sys("create ", "./down file") ;
}

static void deldown (void)
{
  int e = errno ;
  if (unlink("down") == -1 && errno != ENOENT)
    strerr_warnwu2sys("unlink ", "./down file") ;
  errno = e ;
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

static void trystart (void)
{
  cspawn_fileaction fa[2] =
  {
    [0] = { .type = CSPAWN_FA_CLOSE },
    [1] = { .type = CSPAWN_FA_MOVE },
  } ;
  char lkfmt[UINT_FMT] ;
  char const *cargv[7] = { S6_BINPREFIX "s6-setlock", "-d", lkfmt, "--", "./run", servicename, 0 } ;
  size_t orig = 4 ;
  int notifyp[2] = { -1, -1 } ;
  unsigned int lk = 0, notif = 0 ;

  if (read_uint("lock-fd", &lk))
  {
    if (lk > maxfd) strerr_warnw2x("invalid ", "lock-fd") ;
    else
    {
      struct stat st ;
      int islocked ;
      int lfd = open_write(SLCK) ;
      if (lfd == -1)
      {
        settimeout(60) ;
        strerr_warnwu4sys("open ", SLCK, " for writing", " (waiting 60 seconds)") ;
        goto errn ;
      }
      if (fstat(lfd, &st) == -1)
      {
        settimeout(60) ;
        strerr_warnwu3sys("stat ", SLCK, " (waiting 60 seconds)") ;
        fd_close(lfd) ;
        return ;
      }
      if (st.st_size)
      {
        ftruncate(lfd, 0) ;
        strerr_warnw1x("a previous instance of the service wrote to the lock file!") ;
      }
      islocked = fd_islocked(lfd) ;
      if (islocked == -1)
      {
        settimeout(60) ;
        strerr_warnwu3sys("read lock state on ", SLCK, " (waiting 60 seconds)") ;
        fd_close(lfd) ;
        return ;
      }
      if (islocked)
        strerr_warnw1x("another instance of the service is already running, child will block") ;
      fd_close(lfd) ;
      lkfmt[uint_fmt(lkfmt, lk)] = 0 ;
      orig = 0 ;
    }
  }

  if (read_uint("notification-fd", &notif))
  {
    if (notif > maxfd) strerr_warnw2x("invalid ", "notification-fd") ;
    if (!orig && notif == lk)
    {
      settimeout_infinite() ;
      strerr_warnwu1x("start service: notification-fd and lock-fd are the same") ;
      return ;
    }
    if (pipe(notifyp) == -1)
    {
      settimeout(60) ;
      strerr_warnwu2sys("create notification pipe", " (waiting 60 seconds)") ;
      return ;
    }
    fa[0].x.fd = notifyp[0] ;
    fa[1].x.fd2[0] = notif ;
    fa[1].x.fd2[1] = notifyp[1] ;
  }

  status.pid = cspawn(cargv[orig], cargv + orig, (char const *const *)environ, CSPAWN_FLAGS_SELFPIPE_FINISH | CSPAWN_FLAGS_SETSID, fa, notifyp[1] >= 0 ? 2 : 0) ;
  if (!status.pid)
  {
    settimeout(60) ;
    strerr_warnwu3sys("spawn ", cargv[orig], " (waiting 60 seconds)") ;
    goto errn ;
  }

  if (notifyp[1] >= 0)
  {
    fd_close(notifyp[1]) ;
    notifyfd = notifyp[0] ;
  }
  settimeout_infinite() ;
  nextstart = tain_zero ;
  state = UP ;
  status.flagready = 0 ;
  tain_wallclock_read(&status.stamp) ;
  announce() ;
  ftrigw_notifyb_nosig(S6_SUPERVISE_EVENTDIR, "u", 1) ;
  return ;

 errn:
  if (notifyp[1] >= 0)
  {
    fd_close(notifyp[1]) ;
    fd_close(notifyp[0]) ;
  }
}

static void wantdown (void)
{
  status.flagwantup = 0 ;
  announce() ;
}

static void wantup (void)
{
  status.flagwantup = 1 ;
  announce() ;
}

static void wantDOWN (void)
{
  adddown() ;
  wantdown() ;
}

static void wantUP (void)
{
  deldown() ;
  wantup() ;
}

static void downtimeout (void)
{
  if (status.flagwantup) trystart() ;
  else settimeout_infinite() ;
}

static void down_o (void)
{
  wantdown() ;
  trystart() ;
}

static void down_u (void)
{
  wantup() ;
  trystart() ;
}

static void down_U (void)
{
  wantUP() ;
  trystart() ;
}

static int uplastup_z (void)
{
  unsigned int n ;
  char fmt0[UINT_FMT] ;
  char fmt1[UINT_FMT] ;
  char const *cargv[5] = { "finish", fmt0, fmt1, servicename, 0 } ;

  status.wstat = (int)status.pid ;
  status.flagpaused = 0 ;
  status.flagready = 0 ;
  gflags.dying = 0 ;
  tain_wallclock_read(&status.stamp) ;
  if (notifyfd >= 0)
  {
    fd_close(notifyfd) ;
    notifyfd = -1 ;
  }
  fmt0[uint_fmt(fmt0, WIFSIGNALED(status.wstat) ? 256 : WEXITSTATUS(status.wstat))] = 0 ;
  fmt1[uint_fmt(fmt1, WTERMSIG(status.wstat))] = 0 ;

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

  status.pid = cspawn("./finish", cargv, (char const *const *)environ, CSPAWN_FLAGS_SELFPIPE_FINISH | CSPAWN_FLAGS_SETSID, 0, 0) ;
  if (!status.pid)
  {
    if (errno != ENOENT) strerr_warnwu2sys("spawn ", "./finish") ;
    set_down_and_ready("dD", 2) ;
    return 0 ;
  }
  {
    tain tto ;
    unsigned int timeout ;
    if (!read_uint("timeout-finish", &timeout)) timeout = 5000 ;
    if (timeout && tain_from_millisecs(&tto, timeout))
      tain_add_g(&deadline, &tto) ;
    else settimeout_infinite() ;
  }
  status.flagfinishing = 1 ;
  announce() ;
  ftrigw_notifyb_nosig(S6_SUPERVISE_EVENTDIR, "d", 1) ;
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
  if (gflags.dying)
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

static void up_d (void)
{
  tain tto ;
  unsigned int timeout ;
  status.flagwantup = 0 ;
  killr() ;
  killc() ;
  if (!read_uint("timeout-kill", &timeout)) timeout = 0 ;
  if (timeout && tain_from_millisecs(&tto, timeout))
  {
    tain_add_g(&deadline, &tto) ;
    gflags.dying = 1 ;
  }
  else settimeout_infinite() ;
}

static void up_D (void)
{
  adddown() ;
  up_d() ;
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

static action_t_ref const actions[5][27] =
{
  { &downtimeout, &nop, &bail, &bail, &bail, &bail,
    &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop,
    &down_o, &wantdown, &down_u, &wantDOWN, &down_U, &bail, &wantdown, &wantDOWN },
  { &uptimeout, &up_z, &up_term, &up_x, &bail, &sigint,
    &killa, &killb, &killq, &killh, &killk, &killt, &killi, &kill1, &kill2, &killp, &killc, &killy, &killr,
    &wantdown, &up_d, &wantup, &up_D, &wantUP, &up_x, &wantdown, &wantDOWN },
  { &finishtimeout, &finish_z, &finish_x, &finish_x, &bail, &sigint,
    &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop,
    &wantdown, &wantdown, &wantup, &wantDOWN, &wantUP, &finish_x, &wantdown, &wantDOWN },
  { &uptimeout, &lastup_z, &up_d, &closethem, &bail, &sigint,
    &killa, &killb, &killq, &killh, &killk, &killt, &killi, &kill1, &kill2, &killp, &killc, &killy, &killr,
    &wantdown, &up_d, &wantup, &up_D, &wantUP, &closethem, &wantdown, &wantDOWN },
  { &finishtimeout, &lastfinish_z, &nop, &closethem, &bail, &sigint,
    &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop, &nop,
    &wantdown, &wantdown, &wantup, &wantDOWN, &wantUP, &closethem, &wantdown, &wantDOWN }
} ;



/* The main loop.
   It just loops around the iopause(), calling snippets of code in "actions" when needed. */


static inline void handle_notifyfd (void)
{
  char buf[512] ;
  ssize_t r = 1 ;
  while (r > 0)
  {
    r = sanitize_read(fd_read(notifyfd, buf, 512)) ;
    if (r > 0 && memchr(buf, '\n', r))
    {
      tain_addsec_g(&nextstart, 1) ;
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
      size_t pos = byte_chr("abqhkti12pcyroduDUxOQ", 21, c) ;
      if (pos < 21) (*actions[state][V_a + pos])() ;
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
  servicename = argv[1] ;
  {
    size_t proglen = strlen(PROG) ;
    size_t namelen = strlen(argv[1]) ;
    char progname[proglen + namelen + 10] ;
    memcpy(progname, PROG, proglen) ;
    progname[proglen] = ' ' ;
    memcpy(progname + proglen + 1, argv[1], namelen + 1) ;
    memcpy(progname + proglen + 2 + namelen, "(child)", 8) ;
    PROG = progname ;
    if (!fd_sanitize()) strerr_diefu1sys(111, "sanitize stdin and stdout") ;
    {
      struct rlimit rl ;
      if (getrlimit(RLIMIT_NOFILE, &rl) == -1)
        strerr_diefu1sys(111, "getrlimit") ;
      maxfd = rl.rlim_cur ;
    }
    x[1].fd = control_init() ;
    x[0].fd = selfpipe_init() ;
    if (x[0].fd == -1) strerr_diefu1sys(111, "init selfpipe") ;
    if (!sig_altignore(SIGPIPE)) strerr_diefu1sys(111, "ignore SIGPIPE") ;
    {
      sigset_t set ;
      sigemptyset(&set) ;
      sigaddset(&set, SIGCHLD) ;
      sigaddset(&set, SIGTERM) ;
      sigaddset(&set, SIGHUP) ;
      sigaddset(&set, SIGQUIT) ;
      sigaddset(&set, SIGINT) ;
      if (!selfpipe_trapset(&set)) strerr_diefu1sys(111, "trap signals") ;
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
    tain_copynow(&status.stamp) ;
    status.readystamp = status.stamp ;
    announce() ;
    ftrigw_notifyb_nosig(S6_SUPERVISE_EVENTDIR, "s", 1) ;

    while (gflags.cont)
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
