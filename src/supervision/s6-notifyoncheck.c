/* ISC license. */

#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/wait.h>

#include <skalibs/posixplz.h>
#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/bytestr.h>
#include <skalibs/gol.h>
#include <skalibs/strerr.h>
#include <skalibs/tai.h>
#include <skalibs/cspawn.h>
#include <skalibs/djbunix.h>
#include <skalibs/selfpipe.h>
#include <skalibs/iopause.h>
#include <skalibs/exec.h>

#include <s6/s6.h>

#ifdef S6_USE_EXECLINE
#include <execline/config.h>
# define USAGE "s6-notifyoncheck [ -d ] [ -3 fd ] [ -s initialsleep ] [ -T globaltimeout ] [ -t localtimeout ] [ -w waitingtime ] [ -n tries ] [ -c \"checkprog...\" ] prog..."
#else
# define USAGE "s6-notifyoncheck [ -d ] [ -3 fd ] [ -s initialsleep ] [ -T globaltimeout ] [ -t localtimeout ] [ -w waitingtime ] [ -n tries ] prog..."
#endif

#define dieusage() strerr_dieusage(100, USAGE)

enum golb_e
{
  GOLB_DOUBLEFORK = 0x01,
} ;

enum gola_e
{
  GOLA_NOTIF,
  GOLA_INITIALSLEEP,
  GOLA_GLOBALTIMEOUT,
  GOLA_LOCALTIMEOUT,
  GOLA_WAITINGTIME,
  GOLA_TRIES,
#ifdef S6_USE_EXECLINE
  GOLA_CHECKPROG,
#endif
  GOLA_N
} ;

static inline int read_uint (char const *file, unsigned int *fd)
{
  char buf[UINT_FMT + 1] ;
  ssize_t r = openreadnclose_nb(file, buf, UINT_FMT) ;
  if (r == -1) return -1 ;
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

static int handle_event (ftrigr *a, uint32_t id, pid_t pid)
{
  int r ;
  struct iovec v ;
  if (ftrigr_update(a) == -1) strerr_diefu1sys(111, "ftrigr_update") ;
  r = ftrigr_peek(a, id, &v) ;
  if (r == -1) strerr_diefu1sys(111, "ftrigr_check") ;
  if (r)
  {
    if (memchr(v.iov_base, 'd', v.iov_len))
    {
      if (pid) kill(pid, SIGTERM) ;
      ftrigr_ack(a, id) ;
      return 1 ;
    }
    ftrigr_ack(a, id) ;
  }
  return 0 ;
}

int main (int argc, char const *const *argv)
{
  static gol_bool rgolb[] =
  {
    { .so = 'd', .lo = "doublefork", .clear = 0, .set = GOLB_DOUBLEFORK },
  } ;
  static gol_arg rgola[] =
  {
    { .so = '3', .lo = "notification-fd", .i = GOLA_NOTIF },
    { .so = 's', .lo = "initial-sleep", .i = GOLA_INITIALSLEEP },
    { .so = 'T', .lo = "global-timeout", .i = GOLA_GLOBALTIMEOUT },
    { .so = 't', .lo = "local-timeout", .i = GOLA_LOCALTIMEOUT },
    { .so = 'w', .lo = "waiting-time", .i = GOLA_WAITINGTIME },
    { .so = 'n', .lo = "tries", .i = GOLA_TRIES },
#ifdef S6_USE_EXECLINE
    { .so = 'c', .lo = "check-program", .i = GOLA_CHECKPROG },
#endif
  } ;
  uint64_t wgolb = 0 ;
  char const *wgola[GOLA_N] = { 0 } ;
  ftrigr a = FTRIGR_ZERO ;
  iopause_fd x[2] = { { .events = IOPAUSE_READ }, { .events = IOPAUSE_READ } } ;
  char const *childargv[4] = { "./data/check", 0, 0, 0 } ;
  unsigned int fd ;
  int p[2] ;
  tain sleeptto ;
  tain globaldeadline = TAIN_INFINITE_RELATIVE ;
  tain localtto = TAIN_INFINITE_RELATIVE ;
  tain waittto = TAIN_INFINITE_RELATIVE ;
  unsigned int tries ;
  uint32_t id ;
  unsigned int golc ;

  PROG = "s6-notifyoncheck" ;
  golc = GOL_main(argc, argv, rgolb, rgola, &wgolb, wgola) ;
  argc -= golc ; argv += golc ;
  if (!argc) dieusage() ;

  if (wgola[GOLA_NOTIF])
  {
    if (!uint0_scan(wgola[GOLA_NOTIF], &fd))
      strerr_dief2x(100, "notification-fd", " must be an unsigned integer") ;
  }
  else
  {
    int r = read_uint("notification-fd", &fd) ;
    if (r < 0) strerr_diefu2sys(111, "read ", "./notification-fd") ;
    if (!r) strerr_dief2x(100, "invalid ", "./notification-fd") ;
  }
  if (fcntl(fd, F_GETFD) < 0)
    strerr_dief2sys(111, "notification-fd", " sanity check failed") ;
  if (wgola[GOLA_INITIALSLEEP])
  {
    if (!uint0_scan(wgola[GOLA_INITIALSLEEP], &tries))
      strerr_dief2x(100, "initial-sleep", " must be an unsigned integer") ;
  }
  else tries = 10 ;
  if (!tain_from_millisecs(&sleeptto, tries)) dieusage() ;

  if (wgola[GOLA_GLOBALTIMEOUT])
  {
    if (!uint0_scan(wgola[GOLA_GLOBALTIMEOUT], &tries))
      strerr_dief2x(100, "global-timeout", " must be an unsigned integer") ;
    if (tries) tain_from_millisecs(&globaldeadline, tries) ;
  }
  if (wgola[GOLA_LOCALTIMEOUT])
  {
    if (!uint0_scan(wgola[GOLA_LOCALTIMEOUT], &tries))
      strerr_dief2x(100, "local-timeout", " must be an unsigned integer") ;
    if (tries) tain_from_millisecs(&localtto, tries) ;
  }
  if (wgola[GOLA_WAITINGTIME])
  {
    if (!uint0_scan(wgola[GOLA_WAITINGTIME], &tries))
      strerr_dief2x(100, "waiting-time", " must be an unsigned integer") ;
  }
  else tries = 1000 ;
  if (!tain_from_millisecs(&waittto, tries)) dieusage() ;
  if (wgola[GOLA_TRIES])
  {
    if (!uint0_scan(wgola[GOLA_TRIES], &tries))
      strerr_dief2x(100, "waiting-time", " must be an unsigned integer") ;
    if (!tries) tries = UINT_MAX ;
  }
  else tries = 7 ;

#ifdef S6_USE_EXECLINE
  if (wgola[GOLA_CHECKPROG])
  {
    childargv[0] = EXECLINE_EXTBINPREFIX "execlineb" ;
    childargv[1] = "-Pc" ;
    childargv[2] = wgola[GOLA_CHECKPROG] ;
  }
#endif

  {
    int r = s6_svc_ok(".") ;
    if (r < 0) strerr_diefu1sys(111, "sanity-check current service directory") ;
    if (!r) strerr_dief1x(100, "s6-supervise not running.") ;
  }

  if (!tain_now_set_stopwatch_g()) strerr_diefu1sys(111, "tain_now") ;
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
  switch (wgolb & GOLB_DOUBLEFORK ? doublefork() : fork())
  {
    case -1 : strerr_diefu1sys(111, wgolb & GOLB_DOUBLEFORK ? "doublefork" : "fork") ;
    case 0 : break ;
    default:
    {
      char c ;
      close(fd) ;
      if (read(p[0], &c, 1) < 1) strerr_diefu1x(111, "synchronize with child") ;
      xexec(argv) ;
    }
  }


  PROG = "s6-notifyoncheck (child)" ;
  close(p[0]) ;
  if (!ftrigr_startf_g(&a, &globaldeadline))
    strerr_diefu1sys(111, "ftrigr_startf") ;
  if (!ftrigr_subscribe_g(&a, &id, 0, 0, "event", "d", &globaldeadline))
    strerr_diefu1sys(111, "ftrigr_subscribe to event fifodir") ;

  x[0].fd = selfpipe_init() ;
  if (x[0].fd == -1) strerr_diefu1sys(111, "selfpipe_init") ;
  if (selfpipe_trap(SIGCHLD) < 0) strerr_diefu1sys(111, "trap SIGCHLD") ;
  x[1].fd = ftrigr_fd(&a) ;

  if (fd_write(p[1], "", 1) < 1) strerr_diefu1sys(2, "synchronize with parent") ;
  close(p[1]) ;

 /* Loop around a sleep and a ./data/check invocation */

  while (tries == UINT_MAX || tries--)
  {
    tain deadline = globaldeadline ;
    tain localdeadline ;
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
        if (!tain_future(&globaldeadline)) _exit(3) ;
        else break ;
      }
      if (handle_event(&a, id, 0)) _exit(2) ;
    }

    pid = cspawn(childargv[0], childargv, (char const *const *)environ, CSPAWN_FLAGS_SELFPIPE_FINISH, 0, 0) ;
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
          _exit(3) ;
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
            _exit(0) ;
          }
          else break ;
        }
      }
      if (x[1].revents & IOPAUSE_READ && handle_event(&a, id, pid)) _exit(2) ;
    }
  }

  _exit(1) ;
}
