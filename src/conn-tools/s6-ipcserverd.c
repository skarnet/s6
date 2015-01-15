/* ISC license. */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <skalibs/uint.h>
#include <skalibs/gccattributes.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/bytestr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/diuint.h>
#include <skalibs/env.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/iopause.h>
#include <skalibs/webipc.h>

#define USAGE "s6-ipcserverd [ -v verbosity ] [ -1 ] [ -P | -p ] [ -c maxconn ] [ -C localmaxconn ] prog..."

#define ABSOLUTE_MAXCONN 1000

static unsigned int maxconn = 40 ;
static unsigned int localmaxconn = 40 ;
static char fmtmaxconn[UINT_FMT+1] = "/" ;
static char fmtlocalmaxconn[UINT_FMT+1] = "/" ;
static int flaglookup = 1 ;
static unsigned int verbosity = 1 ;
static int cont = 1 ;

static diuint *piduid ;
static unsigned int numconn = 0 ;
static diuint *uidnum ;
static unsigned int uidlen = 0 ;


 /* Utility functions */

static inline void dieusage ()
{
  strerr_dieusage(100, USAGE) ;
}

static inline void X (void)
{
  strerr_dief1x(101, "internal inconsistency. Please submit a bug-report.") ;
}


 /* Lookup primitives */

static unsigned int lookup_diuint (diuint const *tab, unsigned int tablen, unsigned int key)
{
  register unsigned int i = 0 ;
  for (; i < tablen ; i++) if (key == tab[i].left) break ;
  return i ;
}

static inline unsigned int lookup_pid (unsigned int pid)
{
  return lookup_diuint(piduid, numconn, pid) ;
}

static inline unsigned int lookup_uid (unsigned int uid)
{
  return lookup_diuint(uidnum, uidlen, uid) ;
}


 /* Logging */

static inline void log_start (void)
{
  strerr_warni1x("starting") ;
}

static inline void log_exit (void)
{
  strerr_warni1x("exiting") ;
}

static void log_status (void)
{
  char fmt[UINT_FMT] ;
  fmt[uint_fmt(fmt, numconn)] = 0 ;
  strerr_warni3x("status: ", fmt, fmtmaxconn) ;
}

static void log_deny (unsigned int uid, unsigned int gid, unsigned int num)
{
  char fmtuid[UINT_FMT] = "?" ;
  char fmtgid[UINT_FMT] = "?" ;
  char fmtnum[UINT_FMT] = "?" ;
  if (flaglookup)
  {
    fmtuid[uint_fmt(fmtuid, uid)] = 0 ;
    fmtgid[uint_fmt(fmtgid, gid)] = 0 ;
    fmtnum[uint_fmt(fmtnum, num)] = 0 ;
  }
  strerr_warni7sys("deny ", fmtuid, ":", fmtgid, " count ", fmtnum, fmtlocalmaxconn) ;
}

static void log_accept (unsigned int pid, unsigned int uid, unsigned int gid, unsigned int num)
{
  char fmtuidgid[UINT_FMT * 2 + 1] = "?:?" ;
  char fmtpid[UINT_FMT] ;
  char fmtnum[UINT_FMT] = "?" ;
  if (flaglookup)
  {
    register unsigned int n = uint_fmt(fmtuidgid, uid) ;
    fmtuidgid[n++] = ':' ;
    n += uint_fmt(fmtuidgid + n, gid) ;
    fmtuidgid[n] = 0 ;
    fmtnum[uint_fmt(fmtnum, num)] = 0 ;
  }
  fmtpid[uint_fmt(fmtpid, pid)] = 0 ;
  strerr_warni7x("allow ", fmtuidgid, " pid ", fmtpid, " count ", fmtnum, fmtlocalmaxconn) ;
}

static void log_close (unsigned int pid, unsigned int uid, int w)
{
  char fmtpid[UINT_FMT] ;
  char fmtuid[UINT_FMT] = "?" ;
  char fmtw[UINT_FMT] ;
  fmtpid[uint_fmt(fmtpid, pid)] = 0 ;
  if (flaglookup) fmtuid[uint_fmt(fmtuid, uid)] = 0 ;
  fmtw[uint_fmt(fmtw, WIFSIGNALED(w) ? WTERMSIG(w) : WEXITSTATUS(w))] = 0 ;
  strerr_warni6x("end pid ", fmtpid, " uid ", fmtuid, WIFSIGNALED(w) ? " signal " : " exitcode ", fmtw) ;
}


 /* Signal handling */

static void killthem (int sig)
{
  register unsigned int i = 0 ;
  for (; i < numconn ; i++) kill(piduid[i].left, sig) ;
}

static void wait_children (void)
{
  for (;;)
  {
    unsigned int i ;
    int w ;
    register pid_t pid = wait_nohang(&w) ;
    if (pid < 0)
      if (errno != ECHILD) strerr_diefu1sys(111, "wait_nohang") ;
      else break ;
    else if (!pid) break ;
    i = lookup_pid(pid) ;
    if (i < numconn)
    {
      unsigned int uid = piduid[i].right ;
      register unsigned int j = lookup_uid(uid) ;
      if (j >= uidlen) X() ;
      if (!--uidnum[j].right) uidnum[j] = uidnum[--uidlen] ;
      piduid[i] = piduid[--numconn] ;
      if (verbosity >= 2)
      {
        log_close(pid, uid, w) ;
        log_status() ;
      }
    }
  }
}

static void handle_signals (void)
{
  for (;;) switch (selfpipe_read())
  {
    case -1 : strerr_diefu1sys(111, "read selfpipe") ;
    case 0 : return ;
    case SIGCHLD : wait_children() ; break ;
    case SIGTERM :
    {
      if (verbosity >= 2)
        strerr_warni3x("received ", "SIGTERM,", " quitting") ;
      cont = 0 ;
      break ;
    }
    case SIGHUP :
    {
      if (verbosity >= 2)
        strerr_warni5x("received ", "SIGHUP,", " sending ", "SIGTERM+SIGCONT", " to all connections") ;
      killthem(SIGTERM) ;
      killthem(SIGCONT) ;
      break ;
    }
    case SIGQUIT :
    {
      if (verbosity >= 2)
        strerr_warni6x("received ", "SIGQUIT,", " sending ", "SIGTERM+SIGCONT", " to all connections", " and quitting") ;
      cont = 0 ;
      killthem(SIGTERM) ;
      killthem(SIGCONT) ;
      break ;
    }
    case SIGABRT :
    {
      if (verbosity >= 2)
        strerr_warni6x("received ", "SIGABRT,", " sending ", "SIGKILL", " to all connections", " and quitting") ;
      cont = 0 ;
      killthem(SIGKILL) ;
      break ;
    }
    default : X() ;
  }
}


 /* New connection handling */

static void run_child (int, unsigned int, unsigned int, unsigned int, char const *, char const *const *, char const *const *) gccattr_noreturn ;
static void run_child (int s, unsigned int uid, unsigned int gid, unsigned int num, char const *remotepath, char const *const *argv, char const *const *envp)
{
  unsigned int rplen = str_len(remotepath) + 1 ;
  unsigned int n = 0 ;
  char fmt[65 + UINT_FMT * 3 + rplen] ;
  PROG = "s6-ipcserver (child)" ;
  if ((fd_move(0, s) < 0) || (fd_copy(1, 0) < 0))
    strerr_diefu1sys(111, "move fds") ;
  byte_copy(fmt+n, 23, "PROTO=IPC\0IPCREMOTEEUID") ; n += 23 ;
  if (flaglookup)
  {
    fmt[n++] = '=' ;
    n += uint_fmt(fmt+n, uid) ;
  }
  fmt[n++] = 0 ;
  byte_copy(fmt+n, 13, "IPCREMOTEEGID") ; n += 13 ;
  if (flaglookup)
  {
    fmt[n++] = '=' ;
    n += uint_fmt(fmt+n, gid) ;
  }
  fmt[n++] = 0 ;
  byte_copy(fmt+n, 11, "IPCCONNNUM=") ; n += 11 ;
  if (flaglookup) n += uint_fmt(fmt+n, num) ;
  fmt[n++] = 0 ;
  byte_copy(fmt+n, 14, "IPCREMOTEPATH=") ; n += 14 ;
  byte_copy(fmt+n, rplen, remotepath) ; n += rplen ;
  pathexec_r(argv, envp, env_len(envp), fmt, n) ;
  strerr_dieexec(111, argv[0]) ;
}

static void new_connection (int s, char const *remotepath, char const *const *argv, char const *const *envp)
{
  unsigned int uid = 0, gid = 0 ;
  unsigned int num, i ;
  register pid_t pid ;
  if (flaglookup && (ipc_eid(s, &uid, &gid) < 0))
  {
    if (verbosity) strerr_warnwu1sys("ipc_eid") ;
    return ;
  }
  i = lookup_uid(uid) ;
  num = (i < uidlen) ? uidnum[i].right : 0 ;
  if (num >= localmaxconn)
  {
    log_deny(uid, gid, num) ;
    return ;
  }
  pid = fork() ;
  if (pid < 0)
  {
    if (verbosity) strerr_warnwu1sys("fork") ;
    return ;
  }
  else if (!pid)
  {
    selfpipe_finish() ;
    run_child(s, uid, gid, num+1, remotepath, argv, envp) ;
  }

  if (i < uidlen) uidnum[i].right = num + 1 ;
  else
  {
    uidnum[uidlen].left = uid ;
    uidnum[uidlen++].right = 1 ;
  }
  piduid[numconn].left = (unsigned int)pid ;
  piduid[numconn++].right = uid ;
  if (verbosity >= 2)
  {
    log_accept((unsigned int)pid, uid, gid, uidnum[i].right) ;
    log_status() ;
  }
}


 /* And the main */

int main (int argc, char const *const *argv, char const *const *envp)
{
  iopause_fd x[2] = { { .events = IOPAUSE_READ }, { .fd = 0, .events = IOPAUSE_READ | IOPAUSE_EXCEPT } } ;
  PROG = "s6-ipcserverd" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    int flag1 = 0 ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "Pp1c:C:v:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'P' : flaglookup = 0 ; break ;
        case 'p' : flaglookup = 1 ; break ;
        case '1' : flag1 = 1 ; break ;
        case 'c' : if (!uint0_scan(l.arg, &maxconn)) dieusage() ; break ;
        case 'C' : if (!uint0_scan(l.arg, &localmaxconn)) dieusage() ; break ;
        case 'v' : if (!uint0_scan(l.arg, &verbosity)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (!argc || !*argv[0]) dieusage() ;
    {
      struct stat st ;
      if (fstat(0, &st) < 0) strerr_diefu1sys(111, "fstat stdin") ;
      if (!S_ISSOCK(st.st_mode)) strerr_dief1x(100, "stdin is not a socket") ;
    }
    if (coe(0) < 0) strerr_diefu1sys(111, "make socket close-on-exec") ;
    if (flag1)
    {
      if (fcntl(1, F_GETFD) < 0)
        strerr_dief1sys(100, "called with option -1 but stdout said") ;
    }
    else close(1) ;
    if (!maxconn) maxconn = 1 ;
    if (maxconn > ABSOLUTE_MAXCONN) maxconn = ABSOLUTE_MAXCONN ;
    if (!flaglookup || (localmaxconn > maxconn)) localmaxconn = maxconn ;

    x[0].fd = selfpipe_init() ;
    if (x[0].fd == -1) strerr_diefu1sys(111, "create selfpipe") ;
    if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "ignore SIGPIPE") ;
    {
      sigset_t set ;
      sigemptyset(&set) ;
      sigaddset(&set, SIGCHLD) ;
      sigaddset(&set, SIGTERM) ;
      sigaddset(&set, SIGHUP) ;
      sigaddset(&set, SIGQUIT) ;
      sigaddset(&set, SIGABRT) ;
      if (selfpipe_trapset(&set) < 0) strerr_diefu1sys(111, "trap signals") ;
    }

    fmtlocalmaxconn[1+uint_fmt(fmtlocalmaxconn+1, localmaxconn)] = 0 ;
    if (verbosity >= 2)
    {
      fmtmaxconn[1+uint_fmt(fmtmaxconn+1, maxconn)] = 0 ;
      log_start() ;
      log_status() ;
    }
    if (flag1)
    {
      fd_write(1, "\n", 1) ;
      fd_close(1) ;
    }
  }

  {
    diuint inyostack[maxconn + (flaglookup ? maxconn : 1)] ;
    piduid = inyostack ; uidnum = inyostack + maxconn ;

    while (cont)
    {
      if (iopause_g(x, 1 + (numconn < maxconn), 0) < 0)
        strerr_diefu1sys(111, "iopause") ;

      if (x[0].revents & IOPAUSE_EXCEPT) strerr_dief1x(111, "trouble with selfpipe") ;
      if (x[0].revents & IOPAUSE_READ) handle_signals() ;
      if (numconn < maxconn)
      {
        if (x[1].revents & IOPAUSE_EXCEPT) strerr_dief1x(111, "trouble with socket") ;
        if (x[1].revents & IOPAUSE_READ)
        {
          int dummy ;
          char remotepath[IPCPATH_MAX+1] ;
          register int s = ipc_accept(x[1].fd, remotepath, IPCPATH_MAX+1, &dummy) ;
          if (s < 0)
          {
            if (verbosity) strerr_warnwu1sys("accept") ;
          }
          else
          {
            new_connection(s, remotepath, argv, envp) ;
            fd_close(s) ;
          }
        }
      }
    }
  }
  if (verbosity >= 2) log_exit() ;
  return 0 ;
}
