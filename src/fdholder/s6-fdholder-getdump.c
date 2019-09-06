/* ISC license. */

#include <string.h>
#include <limits.h>
#include <skalibs/types.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/env.h>
#include <skalibs/djbunix.h>
#include <skalibs/genalloc.h>
#include <s6/s6-fdholder.h>

#define USAGE "s6-fdholder-getdump [ -t timeout ] socket prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  s6_fdholder_t a = S6_FDHOLDER_ZERO ;
  genalloc dump = GENALLOC_ZERO ;
  tain_t deadline, halfinfinite ;
  PROG = "s6-fdholder-getdump" ;
  {
    unsigned int t = 0 ;
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "t:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&deadline, t) ;
    else deadline = tain_infinite_relative ;
  }
  if (argc < 2) dieusage() ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&deadline, &deadline) ;
  if (!s6_fdholder_start_g(&a, argv[0], &deadline))
    strerr_diefu2sys(111, "connect to a fd-holder daemon at ", argv[0]) ;
  if (!s6_fdholder_getdump_g(&a, &dump, &deadline))
    strerr_diefu1sys(1, "get dump") ;
  s6_fdholder_end(&a) ;
  tain_half(&halfinfinite, &tain_infinite_relative) ;
  tain_add_g(&halfinfinite, &halfinfinite) ;
  {
    size_t n = genalloc_len(s6_fdholder_fd_t, &dump) ;
    size_t pos = 0 ;
    unsigned int i = 0 ;
    char modifs[7 + UINT_FMT + (25 + TIMESTAMP + 4 * UINT_FMT) * n] ;
    if (n > UINT_MAX) strerr_dief1x(100, "dump exceeds maximum size") ;
    memcpy(modifs + pos, "S6_FD#=", 7) ; pos += 7 ;
    pos += uint_fmt(modifs + pos, n) ;
    modifs[pos++] = 0 ;
    for (; i < n ; i++)
    {
      s6_fdholder_fd_t *p = genalloc_s(s6_fdholder_fd_t, &dump) + i ;
      size_t len = strlen(p->id) + 1 ;
      if (uncoe(p->fd) < 0) strerr_diefu1sys(111, "uncoe") ;
      memcpy(modifs + pos, "S6_FD_", 6) ; pos += 6 ;
      pos += uint_fmt(modifs + pos, i) ;
      modifs[pos++] = '=' ;
      pos += uint_fmt(modifs + pos, p->fd) ;
      modifs[pos++] = 0 ;
      memcpy(modifs + pos, "S6_FDID_", 8) ; pos += 8 ;
      pos += uint_fmt(modifs + pos, i) ;
      modifs[pos++] = '=' ;
      memcpy(modifs + pos, p->id, len) ;
      pos += len ;
      memcpy(modifs + pos, "S6_FDLIMIT_", 11) ; pos += 11 ;
      pos += uint_fmt(modifs + pos, i) ;
      if (tain_less(&p->limit, &halfinfinite))
      {
        modifs[pos++] = '=' ;
        pos += timestamp_fmt(modifs + pos, &p->limit) ;
      }
      modifs[pos++] = 0 ;
    }
    xpathexec_r(argv+1, envp, env_len(envp), modifs, pos) ;
  }
}
