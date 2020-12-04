/* ISC license. */

#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include <skalibs/posixplz.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/sgetopt.h>
#include <skalibs/types.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/direntry.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/exec.h>

#include <s6/config.h>
#include <s6/s6-supervise.h>

#define USAGE "s6-svscan [ -c maxservices ] [ -t timeout ] [ -d notif ] [ -X consoleholder ] [ dir ]"
#define dieusage() strerr_dieusage(100, USAGE)

#define CTL S6_SVSCAN_CTLDIR "/control"
#define LCK S6_SVSCAN_CTLDIR "/lock"
#define FINISH_PROG S6_SVSCAN_CTLDIR "/finish"
#define CRASH_PROG S6_SVSCAN_CTLDIR "/crash"
#define SIGNAL_PROG S6_SVSCAN_CTLDIR "/SIG"
#define SIGNAL_PROG_LEN (sizeof(SIGNAL_PROG) - 1)
#define SPECIAL_LOGGER_SERVICE "s6-svscan-log"

#define DIR_RETRY_TIMEOUT 3
#define CHECK_RETRY_TIMEOUT 4

struct svinfo_s
{
  dev_t dev ;
  ino_t ino ;
  tain_t restartafter[2] ;
  pid_t pid[2] ;
  int p[2] ;
  unsigned int flagactive : 1 ;
  unsigned int flaglog : 1 ;
} ;

static struct svinfo_s *services ;
static unsigned int max = 500 ;
static unsigned int n = 0 ;
static tain_t deadline, defaulttimeout ;
static int wantreap = 1 ;
static int wantscan = 1 ;
static unsigned int wantkill = 0 ;
static int cont = 1 ;
static int waitall = 1 ;
static int consoleholder = -1 ;

static void restore_console (void)
{
  if (consoleholder >= 0)
  {
    fd_move(2, consoleholder) ;
    if (fd_copy(1, 2) < 0) strerr_warnwu1sys("restore stdout") ;
  }
}

static void panicnosp (char const *) gccattr_noreturn ;
static void panicnosp (char const *errmsg)
{
  char const *eargv[2] = { CRASH_PROG, 0 } ;
  strerr_warnwu1sys(errmsg) ;
  strerr_warnw2x("executing into ", eargv[0]) ;
  execv(eargv[0], (char *const *)eargv) ;
 /* and if that exec fails, screw it and just die */
  strerr_dieexec(111, eargv[0]) ;
}

static void panic (char const *) gccattr_noreturn ;
static void panic (char const *errmsg)
{
  int e = errno ;
  selfpipe_finish() ;
  restore_console() ;
  errno = e ;
  panicnosp(errmsg) ;
}

static void killthem (void)
{
  unsigned int i = 0 ;
  if (!wantkill) return ;
  for (; i < n ; i++)
  {
    if (!(wantkill & 1) && services[i].flagactive) continue ;
    if (services[i].pid[0])
      kill(services[i].pid[0], (wantkill & 2) ? SIGTERM : SIGHUP) ;
    if (services[i].flaglog && services[i].pid[1])
      kill(services[i].pid[1], (wantkill & 4) ? SIGTERM : SIGHUP) ;
  }
  wantkill = 0 ;
}

static inline void closethem (void)
{
  unsigned int i = 0 ;
  for (; i < n ; i++) if (services[i].flaglog)
  {
    if (services[i].p[1] >= 0) close(services[i].p[1]) ;
    if (services[i].p[0] >= 0) close(services[i].p[0]) ;
  }
}

static inline void waitthem (void)
{
  unsigned int m = 0 ;
  unsigned int i = 0 ;
  pid_t pids[n << 1] ;
  for (; i < n ; i++)
  {
    if (services[i].pid[0])
      pids[m++] = services[i].pid[0] ;
    if (services[i].flaglog && services[i].pid[1])
      pids[m++] = services[i].pid[1] ;
  }
  if (!waitn(pids, m))
    strerr_warnwu1sys("wait for all s6-supervise processes") ;
}

static inline void chld (void)
{
  wantreap = 1 ;
}

static inline void alrm (void)
{
  wantscan = 1 ;
}

static inline void abrt (void)
{
  cont = 0 ;
  waitall = 0 ;
}

static void hup (void)
{
  wantkill = 2 ;
  wantscan = 1 ;
}

static void term (void)
{
  cont = 0 ;
  waitall = 1 ;
  wantkill = 3 ;
}

static void quit (void)
{
  cont = 0 ;
  waitall = 1 ;
  wantkill = 7 ;
}

static void handle_signals (void)
{
  for (;;)
  {
    int sig = selfpipe_read() ;
    switch (sig)
    {
      case -1 : panic("selfpipe_read") ;
      case 0 : return ;
      case SIGCHLD : chld() ; break ;
      case SIGALRM : alrm() ; break ;
      case SIGABRT : abrt() ; break ;
      default :
      {
        char const *name = sig_name(sig) ;
        size_t len = strlen(name) ;
        char fn[SIGNAL_PROG_LEN + len + 1] ;
        char const *const newargv[2] = { fn, 0 } ;
        memcpy(fn, SIGNAL_PROG, SIGNAL_PROG_LEN) ;
        memcpy(fn + SIGNAL_PROG_LEN, name, len + 1) ;
        if (!child_spawn0(newargv[0], newargv, (char const **)environ))
        {
          if (errno != ENOENT) strerr_warnwu2sys("spawn ", newargv[0]) ;
          switch (sig)
          {
            case SIGHUP : hup() ; break ;
            case SIGINT :
            case SIGTERM : term() ; break ;
            case SIGQUIT : quit() ; break ;
          }
        }
      }
    }
  }
}

static void handle_control (int fd)
{
  for (;;)
  {
    char c ;
    ssize_t r = sanitize_read(fd_read(fd, &c, 1)) ;
    if (r < 0) panic("read control pipe") ;
    else if (!r) break ;
    else switch (c)
    {
      case 'z' : chld() ; break ;
      case 'a' : alrm() ; break ;
      case 'b' : abrt() ; break ;
      case 'h' : hup() ; break ;
      case 'i' :
      case 't' : term() ; break ;
      case 'q' : quit() ; break ;
      case 'n' : wantkill = 2 ; break ;
      case 'N' : wantkill = 6 ; break ;
      default :
      {
        char s[2] = { c, 0 } ;
        strerr_warnw2x("received unknown control command: ", s) ;
      }
    }
  }
}


/* First essential function: the reaper.
   s6-svscan must wait() for all children,
   including ones it doesn't know it has.
   Dead active services are flagged to be restarted in 1 second. */

static void reap (void)
{
  tain_t nextscan ;
  if (!wantreap) return ;
  wantreap = 0 ;
  tain_addsec_g(&nextscan, 1) ;
  for (;;)
  {
    int wstat ;
    pid_t r = wait_nohang(&wstat) ;
    if (r < 0)
      if (errno != ECHILD) panic("wait_nohang") ;
      else break ;
    else if (!r) break ;
    else
    {
      unsigned int i = 0 ;
      for (; i < n ; i++)
      {
        if (services[i].pid[0] == r)
        {
          services[i].pid[0] = 0 ;
          services[i].restartafter[0] = nextscan ;
          break ;
        }
        else if (services[i].pid[1] == r)
        {
          services[i].pid[1] = 0 ;
          services[i].restartafter[1] = nextscan ;
          break ;
        }
      }
      if (i == n) continue ;
      if (services[i].flagactive)
      {
        if (tain_less(&nextscan, &deadline)) deadline = nextscan ;
      }
      else
      {
        if (services[i].flaglog)
        {
 /*
    BLACK MAGIC:
     - we need to close the pipe early:
       * as soon as the writer exits so the logger can exit on EOF
       * or as soon as the logger exits so the writer can crash on EPIPE
     - but if the same service gets reactivated before the second
       supervise process exits, ouch: we've lost the pipe
     - so we can't reuse the same service even if it gets reactivated
     - so we're marking a dying service with a closed pipe
     - if the scanner sees a service with p[0] = -1 it won't flag
       it as active (and won't restart the dead supervise)
     - but if the service gets reactivated we want it to restart
       as soon as the 2nd supervise process dies
     - so the scanner marks such a process with p[0] = -2
     - and the reaper triggers a scan when it finds a -2.
 */
          if (services[i].p[0] >= 0)
          {
            fd_close(services[i].p[1]) ; services[i].p[1] = -1 ;
            fd_close(services[i].p[0]) ; services[i].p[0] = -1 ;
          }
          else if (services[i].p[0] == -2) wantscan = 1 ;
        }
        if (!services[i].pid[0] && (!services[i].flaglog || !services[i].pid[1]))
          services[i] = services[--n] ;
      }
    }
  }
}


/* Second essential function: the scanner.
   It monitors the service directories and spawns a supervisor
   if needed. */

static void trystart (unsigned int i, char const *name, int islog)
{
  pid_t pid = fork() ;
  switch (pid)
  {
    case -1 :
      tain_addsec_g(&services[i].restartafter[islog], CHECK_RETRY_TIMEOUT) ;
      strerr_warnwu2sys("fork for ", name) ;
      return ;
    case 0 :
    {
      char const *cargv[3] = { "s6-supervise", name, 0 } ;
      PROG = "s6-svscan (child)" ;
      selfpipe_finish() ;
      if (services[i].flaglog)
        if (fd_move(!islog, services[i].p[!islog]) == -1)
          strerr_diefu2sys(111, "set fds for ", name) ;
      if (consoleholder >= 0
       && !strcmp(name, SPECIAL_LOGGER_SERVICE)
       && fd_move(2, consoleholder) < 0)  /* autoclears coe */
         strerr_diefu1sys(111, "restore console fd for service " SPECIAL_LOGGER_SERVICE) ;
      xexec_a(S6_BINPREFIX "s6-supervise", cargv) ;
    }
  }
  services[i].pid[islog] = pid ;
}

static void retrydirlater (void)
{
  tain_t a ;
  tain_addsec_g(&a, DIR_RETRY_TIMEOUT) ;
  if (tain_less(&a, &deadline)) deadline = a ;
}

static inline void check (char const *name)
{
  struct stat st ;
  size_t namelen ;
  unsigned int i = 0 ;
  if (name[0] == '.') return ;
  if (stat(name, &st) == -1)
  {
    strerr_warnwu2sys("stat ", name) ;
    retrydirlater() ;
    return ;
  }
  if (!S_ISDIR(st.st_mode)) return ;
  namelen = strlen(name) ;
  for (; i < n ; i++) if ((services[i].ino == st.st_ino) && (services[i].dev == st.st_dev)) break ;
  if (i < n)
  {
    if (services[i].flaglog && (services[i].p[0] < 0))
    {
     /* See BLACK MAGIC above. */
      services[i].p[0] = -2 ;
      return ;
    }
  }
  else
  {
    if (n >= max)
    {
      strerr_warnwu3x("start supervisor for ", name, ": too many services") ;
      return ;
    }
    else
    {
      struct stat su ;
      char tmp[namelen + 5] ;
      memcpy(tmp, name, namelen) ;
      memcpy(tmp + namelen, "/log", 5) ;
      if (stat(tmp, &su) < 0)
        if (errno == ENOENT) services[i].flaglog = 0 ;
        else
        {
          strerr_warnwu2sys("stat ", tmp) ;
          retrydirlater() ;
          return ;
        }
      else if (!S_ISDIR(su.st_mode))
        services[i].flaglog = 0 ;
      else
      {
        if (pipecoe(services[i].p) < 0)
        {
          strerr_warnwu1sys("pipecoe") ;
          retrydirlater() ;
          return ;
        }
        services[i].flaglog = 1 ;
      }
      services[i].ino = st.st_ino ;
      services[i].dev = st.st_dev ;
      tain_copynow(&services[i].restartafter[0]) ;
      tain_copynow(&services[i].restartafter[1]) ;
      services[i].pid[0] = 0 ;
      services[i].pid[1] = 0 ;
      n++ ;
    }
  }
  
  services[i].flagactive = 1 ;

  if (services[i].flaglog && !services[i].pid[1])
  {
    if (!tain_future(&services[i].restartafter[1]))
    {
      char tmp[namelen + 5] ;
      memcpy(tmp, name, namelen) ;
      memcpy(tmp + namelen, "/log", 5) ;
      trystart(i, tmp, 1) ;
    }
    else if (tain_less(&services[i].restartafter[1], &deadline))
      deadline = services[i].restartafter[1] ;
  }

  if (!services[i].pid[0])
  {
    if (!tain_future(&services[i].restartafter[0]))
      trystart(i, name, 0) ;
    else if (tain_less(&services[i].restartafter[0], &deadline))
      deadline = services[i].restartafter[0] ;
  }
}

static inline void scan (void)
{
  unsigned int i = 0 ;
  DIR *dir ;
  if (!wantscan) return ;
  wantscan = 0 ;
  tain_add_g(&deadline, &defaulttimeout) ;
  dir = opendir(".") ;
  if (!dir)
  {
    strerr_warnwu1sys("opendir .") ;
    retrydirlater() ;
    return ;
  }
  for (; i < n ; i++) services[i].flagactive = 0 ;
  for (;;)
  {
    direntry *d ;
    errno = 0 ;
    d = readdir(dir) ;
    if (!d) break ;
    check(d->d_name) ;
  }
  if (errno)
  {
    strerr_warnwu1sys("readdir .") ;
    retrydirlater() ;
  }
  dir_close(dir) ;
  for (i = 0 ; i < n ; i++) if (!services[i].flagactive && !services[i].pid[0])
  {
    if (services[i].flaglog)
    {
      if (services[i].pid[1]) continue ;
      if (services[i].p[0] >= 0)
      {
        fd_close(services[i].p[1]) ; services[i].p[1] = -1 ;
        fd_close(services[i].p[0]) ; services[i].p[0] = -1 ;
      }
    }
    services[i] = services[--n] ;
  }
}

static inline int control_init (void)
{
  mode_t m = umask(0) ;
  int fdctl, fdlck, r ;
  if (mkdir(S6_SVSCAN_CTLDIR, 0700) < 0)
  {
    struct stat st ;
    if (errno != EEXIST)
      strerr_diefu1sys(111, "mkdir " S6_SVSCAN_CTLDIR) ;
    if (stat(S6_SVSCAN_CTLDIR, &st) < 0)
      strerr_diefu1sys(111, "stat " S6_SVSCAN_CTLDIR) ;
    if (!S_ISDIR(st.st_mode))
      strerr_dief1x(100, S6_SVSCAN_CTLDIR " exists and is not a directory") ;
  }

  fdlck = open(LCK, O_WRONLY | O_NONBLOCK | O_CREAT | O_CLOEXEC, 0600) ;
  if (fdlck < 0) strerr_diefu1sys(111, "open " LCK) ;
  r = fd_lock(fdlck, 1, 1) ;
  if (r < 0) strerr_diefu1sys(111, "lock " LCK) ;
  if (!r) strerr_dief1x(100, "another instance of s6-svscan is already running on the same directory") ;
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
  iopause_fd x[2] = { { -1, IOPAUSE_READ, 0 }, { -1, IOPAUSE_READ, 0 } } ;
  int notif = -1 ;
  PROG = "s6-svscan" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    unsigned int t = 0 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "c:t:d:X:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'c' : if (uint0_scan(l.arg, &max)) break ;
        case 't' : if (uint0_scan(l.arg, &t)) break ;
        case 'd' : { unsigned int u ; if (!uint0_scan(l.arg, &u)) dieusage() ; notif = u ; break ; }
        case 'X' : { unsigned int u ; if (!uint0_scan(l.arg, &u)) dieusage() ; consoleholder = u ; break ; }
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&defaulttimeout, t) ;
    else defaulttimeout = tain_infinite_relative ;
    if (max < 2) max = 2 ;
  }

  if (notif >= 0)
  {
    if (notif < 3) strerr_dief1x(100, "notification fd must be 3 or more") ;
    if (fcntl(notif, F_GETFD) < 0) strerr_dief1sys(100, "invalid notification fd") ;
  }
  if (consoleholder >= 0)
  {
    if (consoleholder < 3) strerr_dief1x(100, "console holder fd must be 3 or more") ;
    if (fcntl(consoleholder, F_GETFD) < 0) strerr_dief1sys(100, "invalid console holder fd") ;
    if (coe(consoleholder) < 0) strerr_diefu1sys(111, "coe console holder") ;
  }

  if (argc && (chdir(argv[0]) < 0)) strerr_diefu1sys(111, "chdir") ;
  x[1].fd = control_init() ;
  x[0].fd = selfpipe_init() ;
  if (x[0].fd < 0) strerr_diefu1sys(111, "selfpipe_init") ;

  if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "ignore SIGPIPE") ;
  {
    sigset_t set ;
    sigemptyset(&set) ;
    sigaddset(&set, SIGCHLD) ;
    sigaddset(&set, SIGALRM) ;
    sigaddset(&set, SIGABRT) ;
    sigaddset(&set, SIGHUP) ;
    sigaddset(&set, SIGINT) ;
    sigaddset(&set, SIGTERM) ;
    sigaddset(&set, SIGQUIT) ;
    sigaddset(&set, SIGUSR1) ;
    sigaddset(&set, SIGUSR2) ;
#ifdef SIGPWR
    sigaddset(&set, SIGPWR) ;
#endif
#ifdef SIGWINCH
    sigaddset(&set, SIGWINCH) ;
#endif
    if (selfpipe_trapset(&set) < 0) strerr_diefu1sys(111, "trap signals") ;
  }
  if (notif >= 0)
  {
    fd_write(notif, "\n", 1) ;
    fd_close(notif) ;
    notif = -1 ;
  }

  {
    struct svinfo_s blob[max] ; /* careful with that stack, Eugene */
    services = blob ;
    tain_now_set_stopwatch_g() ;

    /* From now on, we must not die.
       Temporize on recoverable errors, and panic on serious ones. */

    while (cont)
    {
      int r ;
      reap() ;
      scan() ;
      killthem() ;
      r = iopause_g(x, 2, &deadline) ;
      if (r < 0) panic("iopause") ;
      else if (!r) wantscan = 1 ;
      else
      {
        if ((x[0].revents | x[1].revents) & IOPAUSE_EXCEPT)
        {
          errno = EIO ;
          panic("check internal pipes") ;
        }
        if (x[0].revents & IOPAUSE_READ) handle_signals() ;
        if (x[1].revents & IOPAUSE_READ) handle_control(x[1].fd) ;
      }
    }


    /* Finish phase. */

    killthem() ;
    closethem() ;
    restore_console() ;
    if (waitall) waitthem() ; else { chld() ; reap() ; }
    selfpipe_finish() ;
  }
  {
    char const *eargv[2] = { FINISH_PROG, 0 } ;
    execv(eargv[0], (char **)eargv) ;
    if (errno != ENOENT) panicnosp("exec finish script " FINISH_PROG) ;
  }
  return 0 ;
}
