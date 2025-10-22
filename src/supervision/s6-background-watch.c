/* ISC license. */

#include <skalibs/sysdeps.h>

#if defined(SKALIBS_HASPRCTL) || defined(SKALIBS_HASPROCCTL) || defined(SKALIBS_HASKEVENT)

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#undef NEEDS_KEVENT
#if defined(SKALIBS_HASPRCTL)
# include <sys/prctl.h>
# define NEEDS_KEVENT 0
#elif defined(SKALIBS_HASPROCCTL)
# include <sys/procctl.h>
# define NEEDS_KEVENT 0
#elif defined (SKALIBS_HASKEVENT)
# include <sys/types.h>
# include <sys/event.h>
# include <sys/time.h>
# include <pthread.h>
# include <fcntl.h>
# include <limits.h>
# include <skalibs/keventbridge.h>
# define NEEDS_KEVENT 1
#endif

#include <skalibs/posixplz.h>
#include <skalibs/types.h>
#include <skalibs/prog.h>
#include <skalibs/gol.h>
#include <skalibs/bytestr.h>
#include <skalibs/strerr.h>
#include <skalibs/tai.h>
#include <skalibs/selfpipe.h>
#include <skalibs/iopause.h>
#include <skalibs/cspawn.h>
#include <skalibs/djbunix.h>


#define USAGE "s6-background-watch [ -t timeout ] [ -d notif ] pidfile prog..."
#define dieusage() strerr_dieusage(100, USAGE)

static int handle_signals_early (pid_t pid, char const *pidfile, char const *prog)
{
  int sig ;
  for (;;) switch (sig = selfpipe_read())
  {
    case -1 : strerr_diefu1sys(111, "read selfpipe") ;
    case 0 : return 0 ;
    case SIGCHLD :
    {
      int wstat ;
      pid_t r = wait_pid_nohang(pid, &wstat) ;
      if (r == -1) strerr_diefu1sys(111, "wait") ;
      if (!r) break ;
      if (WIFEXITED(wstat))
      {
        if (WEXITSTATUS(wstat))
        {
          char fmt[UINT_FMT] ;
          fmt[uint_fmt(fmt, WEXITSTATUS(wstat))] = 0 ;
          unlink_void(pidfile) ;
          strerr_dief3x(wait_estatus(wstat), prog, " (parent) exited ", fmt) ;
        }
        else return 1 ;
      }
      else
      {
        char fmt[UINT_FMT] ;
        fmt[uint_fmt(fmt, WTERMSIG(wstat))] = 0 ;
        unlink_void(pidfile) ;
        strerr_dief3x(wait_estatus(wstat), prog, " (parent) crashed with signal ", fmt) ;
      }
      break ;
    }
    default :
      kill(pid, sig) ;
      break ;
  }
}

static inline int read_file (char const *file, char *buf, size_t n)
{
  ssize_t r = openreadnclose_nb(file, buf, n) ;
  if (r == -1)
  {
    if (errno != ENOENT) strerr_diefu2sys(111, "open ", file) ;
    return 0 ;
  }
  buf[byte_chr(buf, r, '\n')] = 0 ;
  return 1 ;
}

static inline pid_t get_pid_from_pidfile (char const *pidfile, char const *prog)
{ /* for now, assume the pidfile is there once the parent has exited */
  pid_t pid ;
  char buf[128] ;
  if (!read_file(pidfile, buf, 128))
    strerr_diefu4sys(104, prog, " (parent) exited but pidfile ", pidfile, " does not exist") ;
  if (!pid0_scan(buf, &pid))
    strerr_dief3x(104, "pidfile ", pidfile, " does not contain a valid pid") ;
  return pid ;
}

static inline int handle_signals (pid_t pid, int iske, int *code)
{
  int sig ;
  for (;;) switch (sig = selfpipe_read())
  {
    case -1 : strerr_diefu1sys(111, "read selfpipe") ;
    case 0 : return 0 ;
    case SIGCHLD :
    {
      int wstat ;
      pid_t r = wait_pid_nohang(pid, &wstat) ;
      if (r == -1)
        if (errno != ECHILD || !iske) strerr_diefu1sys(111, "wait") ;
        else break ;
      else if (!r) break ;
      *code = wait_estatus(wstat) ;
      return 1 ;
    }
    default :
      kill(pid, sig) ;
      break ;
  }
}

enum rgola_e
{
  GOLA_TIMEOUT,
  GOLA_NOTIF,
  GOLA_N
} ;

static gol_arg const rgola[GOLA_N] =
{
  { .so = 't', .lo = "timeout-ready", .i = GOLA_TIMEOUT },
  { .so = 'd', .lo = "notification-fd", .i = GOLA_NOTIF }
} ;

int main (int argc, char const *const *argv)
{
  iopause_fd x[1 + NEEDS_KEVENT] = { { .events = IOPAUSE_READ } } ;
  tain tto = TAIN_INFINITE_RELATIVE ;
  tain deadline ;
  unsigned int notif = 0 ;
  pid_t pid ;
  
  char const *wgola[GOLA_N] = { 0 } ;
  unsigned int golc ;
  PROG = "s6-background-watch" ;
  golc = gol_main(argc, argv, 0, 0, rgola, GOLA_N, 0, wgola) ;
  argc -= golc ; argv += golc ;
  if (argc < 2) dieusage() ;
  if (wgola[GOLA_TIMEOUT])
  {
    unsigned int t ;
    if (!uint0_scan(wgola[GOLA_TIMEOUT], &t)) dieusage() ;
    if (!tain_from_millisecs(&tto, t)) dieusage() ;
  }
  if (wgola[GOLA_NOTIF])
  {
    if (!uint0_scan(wgola[GOLA_NOTIF], &notif)) dieusage() ;
    if (notif < 3) strerr_dief2x(100, "notification-fd", " cannot be 0, 1 or 2") ;
    if (fcntl(notif, F_GETFD) == -1) strerr_diefu2sys(111, "check ", "notification-fd") ;
    if (coe(notif) == -1) strerr_diefu1sys(111, "coe notification-fd") ;
  }

  {
    sigset_t full ;
    sigfillset(&full) ;
    if (!selfpipe_trapset(&full))
      strerr_diefu1sys(111, "trap all signals") ;
    x[0].fd = selfpipe_fd() ;
  }

#if defined(SKALIBS_HASPRCTL)
  if (prctl(PR_SET_CHILD_SUBREAPER, 1) == -1)
    strerr_diefu1sys(111, "prctl with PR_SET_CHILD_SUBREAPER") ;
#elif defined(SKALIBS_HASPROCCTL)
  if (procctl(P_PID, 0, PROC_REAP_ACQUIRE, 0) == -1)
    strerr_diefu1sys(111, "procctl with PROC_REAP_ACQUIRE") ;
#endif

  pid = cspawn(argv[1], argv + 1, (char const *const *)environ, CSPAWN_FLAGS_SELFPIPE_FINISH, 0, 0) ;
  if (!pid) strerr_diefu2sys(errno == ENOEXEC ? 126 : 127, "spawn ", argv[1]) ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&deadline, &tto) ;
  for (;;)
  {
    int r = iopause_g(x, 1, &deadline) ;
    if (r == -1) strerr_diefu1sys(111, "iopause") ;
    if (!r)
    {
      strerr_warnw2x(argv[1], " (parent) did not exit before timeout-ready, killing it") ;
      kill(pid, SIGKILL) ;
    }
    else if (handle_signals_early(pid, argv[0], argv[1])) break ;
  }

  pid = get_pid_from_pidfile(argv[0], argv[1]) ;
  if (kill(pid, 0) == -1) strerr_diefu1sys(111, "check daemon with a null signal") ;

#if NEEDS_KEVENT
  keventbridge kb = KEVENTBRIDGE_ZERO ;
  x[1].events = IOPAUSE_READ ;
  if (!keventbridge_start(&kb)) strerr_diefu1sys(111, "keventbridge_start") ;
  {
    struct kevent ke ;
    EV_SET(&ke, pid, EVFILT_PROC, EV_ADD | EV_ONESHOT, NOTE_EXIT, 0, 0) ;
    if (keventbridge_write(&kb, &ke, 1) == -1) strerr_diefu1sys(111, "keventbridge_write") ;
  }
#endif

  if (notif)
  {
    write(notif, "\n", 1) ;
    close(notif) ;
  }

  for (;;)
  {
    int r = iopause_g(x, 1 + NEEDS_KEVENT, 0) ;
    if (r == -1) strerr_diefu1sys(111, "iopause") ;
    if (x[0].revents & IOPAUSE_READ)
    {
      int code = 0 ;
      if (handle_signals(pid, NEEDS_KEVENT, &code)) _exit(code) ;
    }
#if NEEDS_KEVENT
    else if (x[1].revents & IOPAUSE_READ)
    {
      struct kevent ke ;
      int r = keventbridge_read(&kb, &ke) ;
      if (r == -1) strerr_diefu1sys(111, "keventbridge_read") ;
      else if (r && (pid_t)ke.ident == pid && ke.filter == EVFILT_PROC && ke.fflags & NOTE_EXIT)
        _exit(wait_estatus(ke.data)) ;
    }
#endif
  }
}

#else

#include <skalibs/prog.h>
#include <skalibs/strerr.h>

int main (int argc, char const *const *argv)
{
  PROG = "s6-background-watch" ;
  strerr_dief1x(112, "system does not meet the requirements to watch auto-backgrounding daemons") ;
}

#endif
