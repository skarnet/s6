/* ISC license. */

#include <skalibs/nonposix.h>

#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <skalibs/gccattributes.h>
#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/envexec.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/socket.h>

#include <skalibs/posixishard.h>

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

union aligner_u
{
  struct cmsghdr cmsghdr ;
  int i ;
} ;

static inline ssize_t fd_sendmsg (int fd, struct msghdr const *hdr)
{
  ssize_t r ;
  int e = errno ;
  do r = sendmsg(fd, hdr, MSG_NOSIGNAL) ;
  while (r == -1 && errno == EINTR) ;
  if (r <= 0) return 0 ;
  errno = e ;
  return 1 ;
}

static inline void notify_systemd (pid_t pid, char const *path, tain const *deadline) gccattr_noreturn ;
static inline void notify_systemd (pid_t pid, char const *path, tain const *deadline)
{
  iopause_fd x = { .events = IOPAUSE_READ } ;
  int p[2] ;
  int fd ;
  size_t n = 26 ;
  char fmt[26 + PID_FMT] = "READY=1\nBARRIER=1\nMAINPID=" ;
  struct sockaddr_un addr = { 0 } ;
  size_t l = strlen(path) ;
  struct iovec v = { .iov_base = fmt, .iov_len = n } ;
  union aligner_u ancilbuf[1 + (CMSG_SPACE(sizeof(int)) - 1) / sizeof(union aligner_u)] ;
  struct msghdr hdr =
  {
    .msg_name = &addr,
    .msg_namelen = sizeof addr,
    .msg_iov = &v,
    .msg_iovlen = 1,
    .msg_control = ancilbuf,
    .msg_controllen = CMSG_SPACE(sizeof(int))
  } ;
  struct cmsghdr *c = CMSG_FIRSTHDR(&hdr) ;
  if (l > IPCPATH_MAX)
  {
    errno = ENAMETOOLONG ;
    strerr_diefu2sys(111, "send a message to ", path) ;
  }
  fd = ipc_datagram_b() ;
  if (fd == -1) strerr_diefu1sys(111, "create socket") ;
  if (pipecoe(p) == -1) strerr_diefu1sys(111, "pipe") ;
  addr.sun_family = AF_UNIX ;
  memcpy(addr.sun_path, path, l+1) ;
  if (path[0] == '@') addr.sun_path[0] = 0 ;
  n += pid_fmt(fmt + n, pid) ;
  fmt[n++] = '\n' ;
  memset(hdr.msg_control, 0, hdr.msg_controllen) ;
  c->cmsg_level = SOL_SOCKET ;
  c->cmsg_type = SCM_RIGHTS ;
  c->cmsg_len = CMSG_LEN(sizeof(int)) ;
  memcpy(CMSG_DATA(c), p+1, sizeof(int)) ;

  if (!fd_sendmsg(fd, &hdr)) strerr_diefu2sys(111, "send notification message to ", path) ;
  fd_close(fd) ;
  fd_close(p[1]) ;
  x.fd = p[0] ;
  fd = iopause_g(&x, 1, deadline) ;
  if (fd == -1) strerr_diefu1sys(111, "iopause") ;
  _exit(fd ? 0 : 99) ;
}

static inline void run_child (int fd, unsigned int timeout, pid_t pid, char const *s) gccattr_noreturn ;
static inline void run_child (int fd, unsigned int timeout, pid_t pid, char const *s)
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
    if (!r) _exit(99) ;
    r = sanitize_read(fd_read(fd, dummy, 4096)) ;
    if (r < 0)
      if (errno == EPIPE) _exit(1) ;
      else strerr_diefu1sys(111, "read from parent") ;
    else if (r && memchr(dummy, '\n', r)) break ;
  }
  fd_close(fd) ;
  notify_systemd(pid, s, &deadline) ;
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
      run_child(p[0], timeout, parent, s) ;
    }
    fd_close(p[0]) ;
    if (fd_move(fd, p[1]) == -1) strerr_diefu1sys(111, "fd_move") ;
    if (wgolb & GOLB_KEEP) xexec(argv) ;
    else xmexec_n(argv, VAR, sizeof(VAR), 1) ;
  }
}
