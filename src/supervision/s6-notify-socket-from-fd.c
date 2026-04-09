/* ISC license. */

#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/envexec.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/socket.h>

#define USAGE "s6-notify-socket-from-fd [ -d fd ] [ -f ] [ -t timeout ] [ -k ] prog..."
#define dieusage() strerr_dieusage(100, USAGE)

#define VAR "NOTIFY_SOCKET"

enum golb_e
{
  GOLB_SINGLEFORK = 0x01,
  GOLB_KEEP = 0x02,
} ;

enum gola_e
{
  GOLA_NOTIF,
  GOLA_TIMEOUT,
  GOLA_N
} ;

static inline int ipc_sendto (int fd, char const *s, size_t len, char const *path)
{
  struct sockaddr_un sa ;
  size_t l = strlen(path) ;
  if (l > IPCPATH_MAX) return (errno = ENAMETOOLONG, 0) ;
  memset(&sa, 0, sizeof sa) ;
  sa.sun_family = AF_UNIX ;
  memcpy(sa.sun_path, path, l+1) ;
  if (path[0] == '@') sa.sun_path[0] = 0 ;
  return sendto(fd, s, len, MSG_NOSIGNAL, (struct sockaddr *)&sa, sizeof sa) >= 0 ;
}

static inline void notify_systemd (pid_t pid, char const *socketpath)
{
  size_t n = 16 ;
  char fmt[16 + PID_FMT] = "READY=1\nMAINPID=" ;
  int fd = ipc_datagram_b() ;
  if (fd == -1) strerr_diefu1sys(111, "create socket") ;
  n += pid_fmt(fmt + n, pid) ;
  fmt[n++] = '\n' ;
  if (!ipc_sendto(fd, fmt, n, socketpath))
    strerr_diefu2sys(111, "send notification message to ", socketpath) ;
  fd_close(fd) ;
}

static inline int run_child (int fd, unsigned int timeout, pid_t pid, char const *s)
{
  char dummy[4096] ;
  iopause_fd x = { .fd = fd, .events = IOPAUSE_READ } ;
  tain deadline = TAIN_INFINITE_RELATIVE ;
  tain_now_g() ;
  if (timeout) tain_from_millisecs(&deadline, timeout) ;
  tain_add_g(&deadline, &deadline) ;
  for (;;)
  {
    int r = iopause_g(&x, 1, &deadline) ;
    if (r == -1) strerr_diefu1sys(111, "iopause") ;
    if (!r) return 99 ;
    r = sanitize_read(fd_read(fd, dummy, 4096)) ;
    if (r < 0)
      if (errno == EPIPE) return 1 ;
      else strerr_diefu1sys(111, "read from parent") ;
    else if (r && memchr(dummy, '\n', r)) break ;
  }
  fd_close(fd) ;
  notify_systemd(pid, s) ;
  return 0 ;
}

int main (int argc, char const *const *argv)
{
  static gol_bool const rgolb[] =
  {
    { .so = 0, .lo = "doublefork", .clear = GOLB_SINGLEFORK, .set = 0 },
    { .so = 'f', .lo = "no-doublefork", .clear = 0, .set = GOLB_SINGLEFORK },
    { .so = 'k', .lo = "keep-environment", .clear = 0, .set = GOLB_KEEP },
  } ;
  static gol_arg const rgola[] =
  {
    { .so = 'd', .lo = "notification-fd", .i = GOLA_NOTIF },
    { .so = 't', .lo = "timeout", .i = GOLA_TIMEOUT },
  } ;
  char const *s = getenv(VAR) ;
  unsigned int fd = 1 ;
  unsigned int timeout = 0 ;
  uint64_t wgolb = 0 ;
  char const *wgola[GOLA_N] = { 0 } ;
  unsigned int golc ;
  PROG = "s6-notify-socket-from-fd" ;

  golc = GOL_main(argc, argv, rgolb, rgola, &wgolb, wgola) ;
  argc -= golc ; argv += golc ;
  if (!argc) dieusage() ;

  if (wgola[GOLA_NOTIF] && !uint0_scan(wgola[GOLA_NOTIF], &fd))
    strerr_dief2x(100, "notification-fd", " must be an unsigned integer") ;
  if (wgola[GOLA_TIMEOUT] && !uint0_scan(wgola[GOLA_TIMEOUT], &timeout))
    strerr_dief2x(100, "timeout", " must be an unsigned integer") ;
  
  if (!s) xexec(argv) ;
  else
  {
    pid_t parent = getpid() ;
    pid_t child ;
    int p[2] ;
    if (pipe(p) == -1) strerr_diefu1sys(111, "pipe") ;
    child = wgolb & GOLB_SINGLEFORK ? fork() : doublefork() ;
    if (child == -1) strerr_diefu1sys(111, wgolb & GOLB_SINGLEFORK ? "fork" : "doublefork") ;
    else if (!child)
    {
      PROG = "s6-notify-socket-from-fd (child)" ;
      fd_close(p[1]) ;
      return run_child(p[0], timeout, parent, s) ;
    }
    fd_close(p[0]) ;
    if (fd_move(fd, p[1]) == -1) strerr_diefu1sys(111, "fd_move") ;
    if (wgolb & GOLB_KEEP) xexec(argv) ;
    else xmexec_n(argv, VAR, sizeof(VAR), 1) ;
  }
}
