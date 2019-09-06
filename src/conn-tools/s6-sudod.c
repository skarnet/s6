/* ISC license. */

#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/sgetopt.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/selfpipe.h>
#include <skalibs/env.h>
#include <skalibs/djbunix.h>
#include <skalibs/unix-timed.h>
#include <skalibs/unixmessage.h>
#include "s6-sudo.h"

#define USAGE "s6-sudod [ -0 ] [ -1 ] [ -2 ] [ -t timeout ] args..."
#define dieusage() strerr_dieusage(100, USAGE)
#define dienomem() strerr_diefu1sys(111, "stralloc_catb")

int main (int argc, char const *const *argv, char const *const *envp)
{
  subgetopt_t l = SUBGETOPT_ZERO ;
  unixmessage_t m ;
  unsigned int nullfds = 0, t = 0 ;
  pid_t pid ;
  size_t envc = env_len(envp) ;
  uint32_t cargc, cenvc, carglen, cenvlen ;
  int spfd ;
  tain_t deadline = TAIN_INFINITE_RELATIVE ;
  PROG = "s6-sudod" ;
  for (;;)
  {
    int opt = subgetopt_r(argc, argv, "012t:", &l) ;
    if (opt < 0) break ;
    switch (opt)
    {
      case '0' : nullfds |= 1 ; break ;
      case '1' : nullfds |= 2 ; break ;
      case '2' : nullfds |= 4 ; break ;
      case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
      default : dieusage() ;
    }
  }
  argc -= l.ind ; argv += l.ind ;
  if (t) tain_from_millisecs(&deadline, t) ;
  if ((ndelay_on(0) < 0) || (ndelay_on(1) < 0))
    strerr_diefu1sys(111, "make socket non-blocking") ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&deadline, &deadline) ;
  buffer_putnoflush(buffer_1small, S6_SUDO_BANNERB, S6_SUDO_BANNERB_LEN) ;
  if (!buffer_timed_flush_g(buffer_1small, &deadline))
    strerr_diefu1sys(111, "write banner to client") ;
  if (unixmessage_timed_receive_g(unixmessage_receiver_0, &m, &deadline) <= 0)
    strerr_diefu1sys(111, "read message from client") ;
  if (m.nfds != 3)
    strerr_dief1x(100, "client did not send 3 fds") ;
  if (m.len < 16 + S6_SUDO_BANNERA_LEN)
    strerr_dief1x(100, "wrong client message") ;
  if (strncmp(m.s, S6_SUDO_BANNERA, S6_SUDO_BANNERA_LEN))
    strerr_dief1x(100, "wrong client banner") ;
  uint32_unpack_big(m.s + S6_SUDO_BANNERA_LEN, &cargc) ;
  uint32_unpack_big(m.s + S6_SUDO_BANNERA_LEN + 4, &cenvc) ;
  uint32_unpack_big(m.s + S6_SUDO_BANNERA_LEN + 8, &carglen) ;
  uint32_unpack_big(m.s + S6_SUDO_BANNERA_LEN + 12, &cenvlen) ;
  if (S6_SUDO_BANNERA_LEN + 16 + carglen + cenvlen != m.len)
    strerr_dief1x(100, "wrong client argc/envlen") ;
  if ((cargc > 131072) || (cenvc > 131072))
    strerr_dief1x(100, "too many args/envvars from client") ;

  if (nullfds & 1)
  {
    close(m.fds[0]) ;
    m.fds[0] = open2("/dev/null", O_RDONLY) ;
    if (m.fds[0] < 0) strerr_diefu2sys(111, "open /dev/null for ", "reading") ;
  }
  if (nullfds & 2)
  {
    close(m.fds[1]) ;
    m.fds[1] = open2("/dev/null", O_WRONLY) ;
    if (m.fds[1] < 0) strerr_diefu2sys(111, "open /dev/null for ", "writing") ;
  }
  if (nullfds & 4)
  {
    close(m.fds[2]) ;
    m.fds[2] = 2 ;
  }
 
  {
    char const *targv[argc + 1 + cargc] ;
    char const *tenvp[envc + 1 + cenvc] ;
    int p[2] ;
    unsigned int i = 0 ;
    for (; i < (unsigned int)argc ; i++) targv[i] = argv[i] ;
    for (i = 0 ; i <= envc ; i++) tenvp[i] = envp[i] ;
    if (!env_make(targv + argc, cargc, m.s + S6_SUDO_BANNERA_LEN + 16, carglen))
    {
      char c = errno ;
      buffer_putnoflush(buffer_1small, &c, 1) ;
      buffer_timed_flush_g(buffer_1small, &deadline) ;
      errno = c ;
      strerr_diefu1sys(111, "make child argv") ;
    }
    if (!env_make(tenvp + envc + 1, cenvc, m.s + S6_SUDO_BANNERA_LEN + 16 + carglen, cenvlen))
    {
      char c = errno ;
      buffer_putnoflush(buffer_1small, &c, 1) ;
      buffer_timed_flush_g(buffer_1small, &deadline) ;
      errno = c ;
      strerr_diefu1sys(111, "make child envp") ;
    }
    targv[argc + cargc] = 0 ;

    for (i = 0 ; i < cenvc ; i++)
    {
      char const *var = tenvp[envc + 1 + i] ;
      unsigned int j = 0 ;
      size_t len = str_chr(var, '=') ;
      if (!var[len])
      {
        char c = EINVAL ;
        buffer_putnoflush(buffer_1small, &c, 1) ;
        buffer_timed_flush_g(buffer_1small, &deadline) ;
        strerr_dief1x(100, "bad environment from client") ;
      }
      for (; j < envc ; j++) if (!strncmp(var, tenvp[j], len+1)) break ;
      if ((j < envc) && !tenvp[j][len+1]) tenvp[j] = var ;
    }

    spfd = selfpipe_init() ;
    if (spfd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
    if (selfpipe_trap(SIGCHLD) < 0) strerr_diefu1sys(111, "trap SIGCHLD") ;
    if (pipe(p) < 0) strerr_diefu1sys(111, "pipe") ;
    if (coe(p[1]) < 0) strerr_diefu1sys(111, "coe pipe") ;
    pid = fork() ;
    if (pid < 0) strerr_diefu1sys(111, "fork") ;
    if (!pid)
    {
      char c ;
      PROG = "s6-sudod (child)" ;
      fd_close(p[0]) ;
      if ((fd_move(2, m.fds[2]) < 0)
       || (fd_move(1, m.fds[1]) < 0)
       || (fd_move(0, m.fds[0]) < 0))
      {
        char c = errno ;
        fd_write(p[1], &c, 1) ;
        strerr_diefu1sys(111, "move fds") ;
      }
      selfpipe_finish() ;
      pathexec0_run(targv, tenvp) ;
      c = errno ;
      fd_write(p[1], &c, 1) ;
      strerr_dieexec(c == ENOENT ? 127 : 126, targv[0]) ;
    }
    fd_close(p[1]) ;
    {
      char c ;
      ssize_t r = fd_read(p[0], &c, 1) ;
      if (r < 0) strerr_diefu1sys(111, "read from child") ;
      if (r)
      {
        buffer_putnoflush(buffer_1small, &c, 1) ;
        buffer_timed_flush_g(buffer_1small, &deadline) ;
        return 111 ;
      }
    }
    fd_close(p[0]) ;
  }

  fd_close(m.fds[0]) ;
  fd_close(m.fds[1]) ;
  if (!(nullfds & 4)) fd_close(m.fds[2]) ;
  unixmessage_receiver_free(unixmessage_receiver_0) ;
  buffer_putnoflush(buffer_1small, "", 1) ;
  if (!buffer_timed_flush_g(buffer_1small, &deadline))
    strerr_diefu1sys(111, "send confirmation to client") ;

  {
    iopause_fd x[2] = { { .fd = 0, .events = 0 }, { .fd = spfd, .events = IOPAUSE_READ } } ;
    int cont = 1 ;
    while (cont)
    {
      if (iopause_g(x, 2, 0) < 0) strerr_diefu1sys(111, "iopause") ;
      if (x[1].revents)
      {
        for (;;)
        {
          int c = selfpipe_read() ;
          if (c < 0) strerr_diefu1sys(111, "read from selfpipe") ;
          else if (!c) break ;
          else if (c == SIGCHLD)
          {
            int wstat ;
            c = wait_pid_nohang(pid, &wstat) ;
            if ((c < 0) && (errno != ECHILD))
              strerr_diefu1sys(111, "wait_pid_nohang") ;
            else if (c > 0)
            {
              char pack[UINT_PACK] ;
              uint_pack_big(pack, (unsigned int)wstat) ;
              buffer_putnoflush(buffer_1small, pack, UINT_PACK) ;
              cont = 0 ;
            }
          }
          else
            strerr_dief1sys(101, "internal inconsistency, please submit a bug-report") ;
        }
      }
      if (x[0].revents && cont)
      {
        kill(pid, SIGTERM) ;
        kill(pid, SIGCONT) ;
        x[0].fd = -1 ;
        return 1 ;
      }
    }
  }
  if (ndelay_off(1) < 0)
    strerr_diefu1sys(111, "set stdout blocking") ;
  if (!buffer_flush(buffer_1small))
    strerr_diefu1sys(111, "write status to client") ;
  return 0 ;
}
