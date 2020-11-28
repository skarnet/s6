/* ISC license. */

#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/wait.h>

#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/bytestr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <skalibs/selfpipe.h>
#include <skalibs/iopause.h>
#include <skalibs/exec.h>

#include <s6/s6.h>

#ifdef S6_USE_EXECLINE
#include <execline/config.h>
#define USAGE "s6-notifyoncheck [ -d ] [ -3 fd ] [ -s initialsleep ] [ -T globaltimeout ] [ -t localtimeout ] [ -w waitingtime ] [ -n tries ] [ -c \"checkprog...\" ] prog..."
#define OPTIONS "d3:s:T:t:w:n:c:"
#else
#define USAGE "s6-notifyoncheck [ -d ] [ -3 fd ] [ -s initialsleep ] [ -T globaltimeout ] [ -t localtimeout ] [ -w waitingtime ] [ -n tries ] prog..."
#define OPTIONS "d3:s:T:t:w:n:"
#endif

#define dieusage() strerr_dieusage(100, USAGE)


static inline int read_uint (char const *file, unsigned int *fd)
{
  char buf[UINT_FMT + 1] ;
  ssize_t r = openreadnclose_nb(file, buf, UINT_FMT) ;
  if (r < 0) return -1 ;
  buf[byte_chr(buf, r, '\n')] = 0 ;
  return !!uint0_scan(buf, fd) ;
}

static inline int handle_signals (pid_t pid, int *w)
{
  int gotit = 0 ;
  for (;;)
  {
    switch (selfpipe_read())
    {
      case -1 : strerr_diefu1sys(111, "selfpipe_read") ;
      case 0 : return gotit ;
      case SIGCHLD :
      {
        int wstat ;
        if (wait_pid_nohang(pid, &wstat) == pid)
        {
          *w = wstat ;
          gotit = 1 ;
        }
        break ;
      }
    }
  }
}

static int handle_event (ftrigr_t *a, uint16_t id, pid_t pid)
{
  int r ;
  char what ;
  if (ftrigr_update(a) < 0) strerr_diefu1sys(111, "ftrigr_update") ;
  r = ftrigr_check(a, id, &what) ;
  if (r < 0) strerr_diefu1sys(111, "ftrigr_check") ;
  if (r && what == 'd')
  {
    if (pid) kill(pid, SIGTERM) ;
    return 1 ;
  }
  return 0 ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{
  ftrigr_t a = FTRIGR_ZERO ;
  iopause_fd x[2] = { { .events = IOPAUSE_READ }, { .events = IOPAUSE_READ } } ;
  char const *childargv[4] = { "./data/check", 0, 0, 0 } ;
#ifdef S6_USE_EXECLINE
  char const *checkprog = 0 ;
#endif
  unsigned int fd ;
  int df = 0 ;
  int autodetect = 1 ;
  int p[2] ;
  tain_t globaldeadline, sleeptto, localtto, waittto ;
  unsigned int tries = 7 ;
  uint16_t id ;
  PROG = "s6-notifyoncheck" ;

  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    unsigned int initialsleep = 10, globaltimeout = 0, localtimeout = 0, waitingtime = 1000 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, OPTIONS, &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'd' : df = 1 ; break ;
        case '3' : if (!uint0_scan(l.arg, &fd)) dieusage() ; autodetect = 0 ; break ;
        case 's' : if (!uint0_scan(l.arg, &initialsleep)) dieusage() ; break ;
        case 'T' : if (!uint0_scan(l.arg, &globaltimeout)) dieusage() ; break ;
        case 't' : if (!uint0_scan(l.arg, &localtimeout)) dieusage() ; break ;
        case 'w' : if (!uint0_scan(l.arg, &waitingtime)) dieusage() ; break ;
        case 'n' : if (!uint0_scan(l.arg, &tries)) dieusage() ; break ;
#ifdef S6_USE_EXECLINE
        case 'c' : checkprog = l.arg ; break ;
#endif
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (!argc) dieusage() ;

    if (!tain_from_millisecs(&sleeptto, initialsleep)) dieusage() ;
    if (globaltimeout) tain_from_millisecs(&globaldeadline, globaltimeout) ;
    else globaldeadline = tain_infinite_relative ;
    if (localtimeout) tain_from_millisecs(&localtto, localtimeout) ;
    else localtto = tain_infinite_relative ;
    if (waitingtime) tain_from_millisecs(&waittto, waitingtime) ;
    else waittto = tain_infinite_relative ;
    if (!tries) tries = UINT_MAX ;
  }

  {
    int r = s6_svc_ok(".") ;
    if (r < 0) strerr_diefu1sys(111, "sanity-check current service directory") ;
    if (!r) strerr_dief1x(100, "s6-supervise not running.") ;
  }
#ifdef S6_USE_EXECLINE
  if (checkprog)
  {
    childargv[0] = EXECLINE_EXTBINPREFIX "execlineb" ;
    childargv[1] = "-Pc" ;
    childargv[2] = checkprog ;
  }
#endif

  if (autodetect)
  {
    int r = read_uint("notification-fd", &fd) ;
    if (r < 0) strerr_diefu2sys(111, "read ", "./notification-fd") ;
    if (!r) strerr_dief2x(100, "invalid ", "./notification-fd") ;
  }
  if (fcntl(fd, F_GETFD) < 0)
    strerr_dief2sys(111, "notification-fd", " sanity check failed") ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&globaldeadline, &globaldeadline) ;


 /*
   Fork, let the parent exec into the daemon, keep working in the child.

   We want the child to die if the parent dies, because no need to keep
  polling a dead service. And another child will be spawned next time the
  service is relaunched by s6-supervise.
   We could keep a pipe from the parent to the child, for death
  notification, but that's an additional fd forever open in the parent,
  which is not good (we need to be 100% transparent).
   So we're using ftrigr to listen to a 'd' event in the servicedir's
  fifodir. It's much heavier, but temporary - it doesn't use permanent
  resources in the daemon - and we're polling anyway, so the user
  doesn't care about being 100% lightweight.

   We need some voodoo synchronization so ftrigr_start can be started
  from the child without a race condition.

 */

  if (pipecoe(p) < 0) strerr_diefu1sys(111, "pipe") ;
  switch (df ? doublefork() : fork())
  {
    case -1: strerr_diefu1sys(111, df ? "doublefork" : "fork") ;
    case 0 : break ;
    default:
    {
      char c ;
      close((int)fd) ;
      if (read(p[0], &c, 1) < 1) strerr_diefu1x(111, "synchronize with child") ;
      xexec_e(argv, envp) ;
    }
  }


  PROG = "s6-notifyoncheck (child)" ;
  close(p[0]) ;
  if (!ftrigr_startf_g(&a, &globaldeadline))
    strerr_diefu1sys(111, "ftrigr_startf") ;
  id = ftrigr_subscribe_g(&a, "event", "d", 0, &globaldeadline) ;
  if (!id) strerr_diefu1sys(111, "ftrigr_subscribe to event fifodir") ;

  x[0].fd = selfpipe_init() ;
  if (x[0].fd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
  if (selfpipe_trap(SIGCHLD) < 0) strerr_diefu1sys(111, "trap SIGCHLD") ;
  x[1].fd = ftrigr_fd(&a) ;

  if (fd_write(p[1], "", 1) < 1) strerr_diefu1sys(2, "synchronize with parent") ;
  close(p[1]) ;

 /* Loop around a sleep and a ./data/check invocation */

  while (tries == UINT_MAX || tries--)
  {
    tain_t deadline = globaldeadline ;
    tain_t localdeadline ;
    pid_t pid ;

    tain_add_g(&localdeadline, &sleeptto) ;
    sleeptto = waittto ;
    if (tain_less(&localdeadline, &deadline)) deadline = localdeadline ;
    for (;;)
    {
      int r = iopause_g(x+1, 1, &deadline) ;
      if (r < 0) strerr_diefu1sys(111, "iopause") ;
      if (!r)
      {
        if (!tain_future(&globaldeadline)) return 3 ;
        else break ;
      }
      if (handle_event(&a, id, 0)) return 2 ;
    }

    pid = child_spawn0(childargv[0], childargv, envp) ;
    if (!pid)
    {
      strerr_warnwu2sys("spawn ", childargv[0]) ;
      continue ;
    }
    deadline = globaldeadline ;
    tain_add_g(&localdeadline, &localtto) ;
    if (tain_less(&localdeadline, &deadline)) deadline = localdeadline ;
    for (;;)
    {
      int r = iopause_g(x, 2, &deadline) ;
      if (r < 0) strerr_diefu1sys(111, "iopause") ;
      if (!r)
      {
        if (!tain_future(&globaldeadline))
        {
          kill(pid, SIGTERM) ;
          return 3 ;
        }
        else break ;
      }
      if (x[0].revents & IOPAUSE_READ)
      {
        int wstat ;
        if (handle_signals(pid, &wstat))
        {
          if (WIFEXITED(wstat) && !WEXITSTATUS(wstat))
          {
            fd_write((int)fd, "\n", 1) ;
            return 0 ;
          }
          else break ;
        }
      }
      if (x[1].revents & IOPAUSE_READ && handle_event(&a, id, pid)) return 2 ;
    }
  }

  return 1 ;
}
