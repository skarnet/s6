/* ISC license. */

#include <skalibs/nonposix.h>

#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <skalibs/posixplz.h>
#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/envexec.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/socket.h>

#define USAGE "s6-notify-fd-from-socket [ -f ] [ -3 fd ] [ -t timeout ] prog..."
#define dieusage() strerr_dieusage(100, USAGE)

enum golb_e
{
  GOLB_SINGLEFORK = 0x01,
} ;

enum gola_e
{
  GOLA_NOTIF,
  GOLA_TIMEOUT,
  GOLA_N
} ;

static void bindit (int sock, char *name)
{
  struct sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = "" } ;
  socklen_t addrlen = sizeof(struct sockaddr_un) ;
  if (bind(sock, (struct sockaddr *)&addr, sizeof(sa_family_t)) == -1) /* autobind */
    strerr_diefu1sys(111, "bind") ;
  if (getsockname(sock, (struct sockaddr *)&addr, &addrlen) == -1)
    strerr_diefu1sys(111, "getsockname") ;
  if (addr.sun_path[0])
    strerr_diefu1x(111, "autobind to an abstract socket") ;
  memcpy(name, addr.sun_path + 1, 5) ;
  name[5] = 0 ;
}

static inline int run_child (int sock, int fd, unsigned int timeout)
{
  char buf[8192] ;
  iopause_fd x = { .fd = fd, .events = IOPAUSE_READ } ;
  int found = 0 ;
  tain deadline = TAIN_INFINITE_RELATIVE ;
  tain_now_g() ;
  if (timeout) tain_from_millisecs(&deadline, timeout) ;
  tain_add_g(&deadline, &deadline) ;
  while (!found)
  {
    int r = iopause_g(&x, 1, &deadline) ;
    if (r == -1) strerr_diefu1sys(111, "iopause") ;
    if (!r) strerr_dief1x(99, "timed out waiting for notification") ;
    r = sanitize_read(fd_recv(sock, buf, 8191, 0)) ;
    if (r == -1)
    {
      if (errno == EPIPE) _exit(0) ;
      else strerr_diefu1sys(111, "recv") ;
    }
    if (r)
    {
      buf[r++] = 0 ;
      if (!strncmp(buf, "READY=1\n", 8) || strstr(buf, "\nREADY=1\n"))
        found = 1 ;
    }
  }
  fd_write(fd, "\n", 1) ;
  _exit(0) ;
}

static inline int read_uint (char const *file, unsigned int *fd)
{
  char buf[UINT_FMT + 1] ;
  ssize_t r = openreadnclose_nb(file, buf, UINT_FMT) ;
  if (r == -1) return -1 ;
  buf[byte_chr(buf, r, '\n')] = 0 ;
  return !!uint0_scan(buf, fd) ;
}

int main (int argc, char const *const *argv)
{
  static gol_bool const rgolb[] =
  {
    { .so = 'f', .lo = "no-doublefork", .clear = 0, .set = GOLB_SINGLEFORK },
    { .so = 0, .lo = "doublefork", .clear = GOLB_SINGLEFORK, .set = 0 },
  } ;
  static gol_arg const rgola[] =
  {
    { .so = '3', .lo = "notification-fd", .i = GOLA_NOTIF },
    { .so = 't', .lo = "timeout", .i = GOLA_TIMEOUT },
  } ;
  unsigned int fd = 0 ;
  unsigned int timeout = 0 ;
  uint64_t wgolb = 0 ;
  char const *wgola[GOLA_N] = { 0 } ;
  pid_t pid ;
  int sock ;
  unsigned int golc ;
  char modif[21] = "NOTIFY_SOCKET=@XXXXX" ;
  PROG = "s6-notify-fd-from-socket" ;

  golc = GOL_main(argc, argv, rgolb, rgola, &wgolb, wgola) ;
  argc -= golc ; argv += golc ;
  if (!argc) dieusage() ;

  if (wgola[GOLA_TIMEOUT] && !uint0_scan(wgola[GOLA_TIMEOUT], &timeout))
    strerr_dief2x(100, "timeout", " must be an unsigned integer") ;
  if (wgola[GOLA_NOTIF])
  {
    if (!uint0_scan(wgola[GOLA_NOTIF], &fd))
      strerr_dief2x(100, "notification-fd", " must be an unsigned integer") ;
  }
  else
  {
    int r = read_uint("notification-fd", &fd) ;
    if (r == -1) strerr_diefu2sys(111, "read ", "./notification-fd") ;
    if (!r) strerr_dief2x(100, "invalid ", "./notification-fd") ;
  }
  if (fcntl(fd, F_GETFD) == -1)
    strerr_dief2sys(111, "notification-fd", " sanity check failed") ;

  sock = ipc_datagram_nbcoe() ;
  if (sock == -1) strerr_diefu1sys(111, "create socket") ;
  bindit(sock, modif + 15) ;

  pid = wgolb & GOLB_SINGLEFORK ? fork() : doublefork() ;
  if (pid == -1) strerr_diefu1sys(111, wgolb & GOLB_SINGLEFORK ? "fork" : "doublefork") ;
  if (!pid) run_child(sock, fd, timeout) ;
  fd_close(sock) ;
  fd_close(fd) ;
  xmexec_n(argv, modif, 21, 1) ;
}
