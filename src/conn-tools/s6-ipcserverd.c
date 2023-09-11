/* ISC license. */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <skalibs/posixplz.h>
#include <skalibs/types.h>
#include <skalibs/gccattributes.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/iopause.h>
#include <skalibs/socket.h>
#include <skalibs/env.h>
#include <skalibs/cspawn.h>

#define USAGE "s6-ipcserverd [ -v verbosity ] [ -1 ] [ -P | -p ] [ -c maxconn ] [ -C localmaxconn ] prog..."

#define ABSOLUTE_MAXCONN 16384

static unsigned int maxconn = 40 ;
static unsigned int localmaxconn = 40 ;
static char fmtmaxconn[UINT_FMT+1] = "/" ;
static char fmtlocalmaxconn[UINT_FMT+1] = "/" ;
static int flaglookup = 1 ;
static unsigned int verbosity = 1 ;
static int cont = 1 ;

typedef struct piduid_s piduid_t, *piduid_t_ref ;
struct piduid_s
{
  pid_t left ;
  uid_t right ;
} ;

typedef struct uidnum_s uidnum_t, *uidnum_t_ref ;
struct uidnum_s
{
  uid_t left ;
  unsigned int right ;
} ;

static piduid_t *piduid ;
static unsigned int numconn = 0 ;
static uidnum_t *uidnum ;
static unsigned int uidlen = 0 ;

static inline void dieusage ()
{
  strerr_dieusage(100, USAGE) ;
}

static inline void X (void)
{
  strerr_dief1x(101, "internal inconsistency. Please submit a bug-report.") ;
}

static unsigned int lookup_pid (pid_t pid)
{
  unsigned int i = 0 ;
  for (; i < numconn ; i++) if (pid == piduid[i].left) break ;
  return i ;
}

static inline unsigned int lookup_uid (uid_t uid)
{
  unsigned int i = 0 ;
  for (; i < uidlen ; i++) if (uid == uidnum[i].left) break ;
  return i ;
}

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

static inline void log_deny (uid_t uid, gid_t gid, unsigned int num)
{
  char fmtuid[UID_FMT] = "?" ;
  char fmtgid[GID_FMT] = "?" ;
  char fmtnum[UINT_FMT] = "?" ;
  if (flaglookup)
  {
    fmtuid[uid_fmt(fmtuid, uid)] = 0 ;
    fmtgid[gid_fmt(fmtgid, gid)] = 0 ;
    fmtnum[uint_fmt(fmtnum, num)] = 0 ;
  }
  strerr_warni7sys("deny ", fmtuid, ":", fmtgid, " count ", fmtnum, fmtlocalmaxconn) ;
}

static inline void log_accept (pid_t pid, uid_t uid, gid_t gid, unsigned int num)
{
  char fmtuidgid[UID_FMT + GID_FMT + 1] = "?:?" ;
  char fmtpid[UINT_FMT] ;
  char fmtnum[UINT_FMT] = "?" ;
  if (flaglookup)
  {
    size_t n = uid_fmt(fmtuidgid, uid) ;
    fmtuidgid[n++] = ':' ;
    n += gid_fmt(fmtuidgid + n, gid) ;
    fmtuidgid[n] = 0 ;
    fmtnum[uint_fmt(fmtnum, num)] = 0 ;
  }
  fmtpid[pid_fmt(fmtpid, pid)] = 0 ;
  strerr_warni7x("allow ", fmtuidgid, " pid ", fmtpid, " count ", fmtnum, fmtlocalmaxconn) ;
}

static inline void log_close (pid_t pid, uid_t uid, int w)
{
  char fmtpid[PID_FMT] ;
  char fmtuid[UID_FMT] = "?" ;
  char fmtw[UINT_FMT] ;
  fmtpid[pid_fmt(fmtpid, pid)] = 0 ;
  if (flaglookup) fmtuid[uid_fmt(fmtuid, uid)] = 0 ;
  fmtw[uint_fmt(fmtw, WIFSIGNALED(w) ? WTERMSIG(w) : WEXITSTATUS(w))] = 0 ;
  strerr_warni6x("end pid ", fmtpid, " uid ", fmtuid, WIFSIGNALED(w) ? " signal " : " exitcode ", fmtw) ;
}

static void killthem (int sig)
{
  unsigned int i = 0 ;
  for (; i < numconn ; i++) kill(piduid[i].left, sig) ;
}

static inline void wait_children (void)
{
  for (;;)
  {
    unsigned int i ;
    int w ;
    pid_t pid = wait_nohang(&w) ;
    if (pid < 0)
      if (errno != ECHILD) strerr_diefu1sys(111, "wait_nohang") ;
      else break ;
    else if (!pid) break ;
    i = lookup_pid(pid) ;
    if (i < numconn)
    {
      uid_t uid = piduid[i].right ;
      unsigned int j = lookup_uid(uid) ;
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

static inline void handle_signals (void)
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

static void new_connection (int s, char const *remotepath, char const *const *argv, char const *const *envp, size_t envlen)
{
  uid_t uid = 0 ;
  gid_t gid = 0 ;
  size_t m = 0 ;
  size_t rplen = strlen(remotepath) + 1 ;
  pid_t pid ;
  unsigned int num, i ;
  cspawn_fileaction fa[2] =
  {
    [0] = { .type = CSPAWN_FA_MOVE, .x = { .fd2 = { [0] = 0, [1] = s } } },
    [1] = { .type = CSPAWN_FA_COPY, .x = { .fd2 = { [0] = 1, [1] = 0 } } }
  } ;
  char const *newenvp[envlen + 6] ;
  char fmt[65 + UID_FMT + GID_FMT + UINT_FMT + rplen] ;

  if (flaglookup && (getpeereid(s, &uid, &gid) < 0))
  {
    if (verbosity) strerr_warnwu1sys("getpeereid") ;
    return ;
  }
  i = lookup_uid(uid) ;
  num = (i < uidlen) ? uidnum[i].right : 0 ;
  if (num >= localmaxconn)
  {
    log_deny(uid, gid, num) ;
    return ;
  }

  memcpy(fmt + m, "PROTO=IPC\0IPCREMOTEEUID", 23) ; m += 23 ;
  if (flaglookup)
  {
    fmt[m++] = '=' ;
    m += uid_fmt(fmt + m, uid) ;
  }
  fmt[m++] = 0 ;
  memcpy(fmt + m, "IPCREMOTEEGID", 13) ; m += 13 ;
  if (flaglookup)
  {
    fmt[m++] = '=' ;
    m += gid_fmt(fmt + m, gid) ;
  }
  fmt[m++] = 0 ;
  memcpy(fmt + m, "IPCCONNNUM=", 11) ; m += 11 ;
  if (flaglookup) m += uint_fmt(fmt + m, num) ;
  fmt[m++] = 0 ;
  memcpy(fmt + m, "IPCREMOTEPATH=", 14) ; m += 14 ;
  memcpy(fmt + m, remotepath, rplen) ; m += rplen ;
  env_mergen(newenvp, envlen + 6, envp, envlen, fmt, m, 5) ;
  pid = cspawn(argv[0], argv, newenvp, CSPAWN_FLAGS_SELFPIPE_FINISH, fa, 2) ;
  if (!pid)
  {
    if (verbosity) strerr_warnwu2sys("spawn ", argv[0]) ;
    return ;
  }

  if (i < uidlen) uidnum[i].right = num + 1 ;
  else
  {
    uidnum[uidlen].left = uid ;
    uidnum[uidlen++].right = 1 ;
  }
  piduid[numconn].left = pid ;
  piduid[numconn++].right = uid ;
  if (verbosity >= 2)
  {
    log_accept(pid, uid, gid, uidnum[i].right) ;
    log_status() ;
  }
}

int main (int argc, char const *const *argv)
{
  iopause_fd x[2] = { { .events = IOPAUSE_READ }, { .fd = 0, .events = IOPAUSE_READ | IOPAUSE_EXCEPT } } ;
  PROG = "s6-ipcserverd" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    int flag1 = 0 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "Pp1c:C:v:", &l) ;
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
    if (!sig_altignore(SIGPIPE)) strerr_diefu1sys(111, "ignore SIGPIPE") ;
    {
      sigset_t set ;
      sigemptyset(&set) ;
      sigaddset(&set, SIGCHLD) ;
      sigaddset(&set, SIGTERM) ;
      sigaddset(&set, SIGHUP) ;
      sigaddset(&set, SIGQUIT) ;
      sigaddset(&set, SIGABRT) ;
      if (!selfpipe_trapset(&set)) strerr_diefu1sys(111, "trap signals") ;
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
    piduid_t inyostack0[maxconn] ;
    uidnum_t inyostack1[flaglookup ? maxconn : 1] ;
    size_t envlen = env_len((char const *const *)environ) ;
    piduid = inyostack0 ;
    uidnum = inyostack1 ;

    while (cont)
    {
      if (iopause_g(x, 1 + (numconn < maxconn), 0) < 0)
        strerr_diefu1sys(111, "iopause") ;

      if (x[0].revents & IOPAUSE_EXCEPT) strerr_dief1x(111, "trouble with selfpipe") ;
      if (x[0].revents & IOPAUSE_READ) { handle_signals() ; continue ; }
      if (numconn < maxconn)
      {
        if (x[1].revents & IOPAUSE_EXCEPT) strerr_dief1x(111, "trouble with socket") ;
        if (x[1].revents & IOPAUSE_READ)
        {
          int dummy ;
          char remotepath[IPCPATH_MAX+1] ;
          int sock = ipc_accept(x[1].fd, remotepath, IPCPATH_MAX+1, &dummy) ;
          if (sock < 0)
          {
            if (verbosity) strerr_warnwu1sys("accept") ;
          }
          else
          {
            new_connection(sock, remotepath, argv, (char const *const *)environ, envlen) ;
            fd_close(sock) ;
          }
        }
      }
    }
  }
  if (verbosity >= 2) log_exit() ;
  return 0 ;
}
