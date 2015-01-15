/* ISC license. */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <skalibs/uint.h>
#include <skalibs/sgetopt.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <skalibs/tai.h>
#include <skalibs/env.h>
#include <skalibs/unix-timed.h>
#include <skalibs/unixmessage.h>
#include "s6-sudo.h"

#define USAGE "s6-sudoc [ -e ] [ -t timeoutconn ] [ -T timeoutrun ] [ args... ]"
#define dieusage() strerr_dieusage(100, USAGE)
#define dienomem() strerr_diefu1sys(111, "stralloc_catb")

int main (int argc, char const *const *argv, char const *const *envp)
{
  char buf6[64] ;
  buffer b6 = BUFFER_INIT(&buffer_read, 6, buf6, 64) ;
  unixmessage_sender_t b7 = UNIXMESSAGE_SENDER_INIT(7) ;
  subgetopt_t l = SUBGETOPT_ZERO ;
  unsigned int t = 0, T = 0 ;
  int doenv = 1 ;
  
  tain_t deadline = TAIN_INFINITE_RELATIVE ;
  PROG = "s6-sudoc" ;
  for (;;)
  {
    register int opt = subgetopt_r(argc, argv, "et:T:", &l) ;
    if (opt < 0) break ;
    switch (opt)
    {
      case 'e' : doenv = 0 ; break ;
      case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
      case 'T' : if (!uint0_scan(l.arg, &T)) dieusage() ; break ;
      default : dieusage() ;
    }
  }
  argc -= l.ind ; argv += l.ind ;
  if (t) tain_from_millisecs(&deadline, t) ;
  if ((ndelay_on(6) < 0) || (ndelay_on(7) < 0))
    strerr_diefu1sys(111, "make socket non-blocking") ;
  if (!fd_sanitize() || !fd_ensure_open(2, 1))
    strerr_diefu1sys(111, "sanitize stdin/stdout/stderr") ;

  tain_now_g() ;
  tain_add_g(&deadline, &deadline) ;
  {
    char tmp[S6_SUDO_BANNERB_LEN] ;
    if (buffer_timed_get_g(&b6, tmp, S6_SUDO_BANNERB_LEN, &deadline) < S6_SUDO_BANNERB_LEN)
      strerr_diefu1sys(111, "read banner from s6-sudod") ;
    if (str_diffn(tmp, S6_SUDO_BANNERB, S6_SUDO_BANNERB_LEN))
      strerr_dief1x(100, "wrong server banner") ;
  }
  {
    int fds[3] = { 0, 1, 2 } ;
    char pack[16] ;
    siovec_t v[4] = {
      { .s = S6_SUDO_BANNERA, .len = S6_SUDO_BANNERA_LEN },
      { .s = pack, .len = 16 },
      { .s = 0, .len = 0 },
      { .s = 0, .len = 0 } } ;
    unixmessage_v_t mv = { .v = v, .vlen = 4, .fds = fds, .nfds = 3 } ;
    stralloc sa = STRALLOC_ZERO ;
    unsigned int envlen = doenv ? env_len(envp) : 0 ;
    uint32_pack_big(pack, (uint32)argc) ;
    uint32_pack_big(pack + 4, (uint32)envlen) ;
    if (!env_string(&sa, argv, argc)) dienomem() ;
    v[2].len = sa.len ;
    uint32_pack_big(pack + 8, (uint32)v[2].len) ;
    if (doenv)
    {
      if (!env_string(&sa, envp, envlen)) dienomem() ;
      v[3].len = sa.len - v[2].len ;
    }
    uint32_pack_big(pack + 12, (uint32)v[3].len) ;
    v[2].s = sa.s ;
    v[3].s = sa.s + v[2].len ;
    if (!unixmessage_putv_and_close(&b7, &mv, (unsigned char const *)"\003"))
      strerr_diefu1sys(111, "unixmessage_putv") ;
    stralloc_free(&sa) ;
  }
  if (!unixmessage_sender_timed_flush_g(&b7, &deadline))
    strerr_diefu1sys(111, "send args to server") ;
  unixmessage_sender_free(&b7) ;
  {
    char c ;
    if (buffer_timed_get_g(&b6, &c, 1, &deadline) < 1)
      strerr_diefu1sys(111, "get confirmation from server") ;
    if (c)
    {
      errno = c ;
      strerr_diefu1sys(111, "start privileged program: server answered: ") ;
    }
  }

  if (T) tain_from_millisecs(&deadline, T) ; else deadline = tain_infinite_relative ;
  tain_add_g(&deadline, &deadline) ;
  {
    char pack[UINT_PACK] ;
    if (buffer_timed_get_g(&b6, pack, UINT_PACK, &deadline) < UINT_PACK)
      strerr_diefu1sys(111, "get exit status from server") ;
    uint_unpack_big(pack, &t) ;
  }
  if (WIFSIGNALED(t)) raise(WTERMSIG(t)) ;
  return WEXITSTATUS(t) ;
}
