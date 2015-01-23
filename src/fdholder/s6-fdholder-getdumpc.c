/* ISC license. */

#include <skalibs/uint.h>
#include <skalibs/bytestr.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <skalibs/genalloc.h>
#include <s6/s6-fdholder.h>

#define USAGE "s6-fdholder-getdumpc [ -t timeout ] prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  s6_fdholder_t a = S6_FDHOLDER_ZERO ;
  genalloc dump = GENALLOC_ZERO ;
  tain_t deadline, halfinfinite ;
  PROG = "s6-fdholder-getdumpc" ;
  {
    unsigned int t = 0 ;
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "t:", &l) ;
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
  if (!argc) dieusage() ;

  s6_fdholder_init(&a, 6) ;
  tain_now_g() ;
  tain_add_g(&deadline, &deadline) ;
  if (!s6_fdholder_getdump_g(&a, &dump, &deadline))
    strerr_diefu1sys(111, "get dump") ;
  s6_fdholder_free(&a) ;
  tain_half(&halfinfinite, &tain_infinite_relative) ;
  tain_add_g(&halfinfinite, &halfinfinite) ;
  {
    unsigned int n = genalloc_len(s6_fdholder_fd_t, &dump) ;
    unsigned int pos = 0, i = 0 ;
    char modifs[7 + UINT_FMT + (25 + TIMESTAMP + 4 * UINT_FMT) * n] ;
    byte_copy(modifs + pos, 7, "S6_FD#=") ; pos += 7 ;
    pos += uint_fmt(modifs + pos, n) ;
    modifs[pos++] = 0 ;
    for (; i < n ; i++)
    {
      s6_fdholder_fd_t *p = genalloc_s(s6_fdholder_fd_t, &dump) + i ;
      unsigned int len = str_len(p->id) + 1 ;
      if (uncoe(p->fd) < 0) strerr_diefu1sys(111, "uncoe") ;
      byte_copy(modifs + pos, 6, "S6_FD_") ; pos += 6 ;
      pos += uint_fmt(modifs + pos, i) ;
      modifs[pos++] = '=' ;
      pos += uint_fmt(modifs + pos, p->fd) ;
      modifs[pos++] = 0 ;
      byte_copy(modifs + pos, 8, "S6_FDID_") ; pos += 8 ;
      pos += uint_fmt(modifs + pos, i) ;
      modifs[pos++] = '=' ;
      byte_copy(modifs + pos, len, p->id) ;
      pos += len ;
      if (tain_less(&p->limit, &halfinfinite))
      {
        byte_copy(modifs + pos, 11, "S6_FDLIMIT_") ; pos += 11 ;
        pos += uint_fmt(modifs + pos, i) ;
        modifs[pos++] = '=' ;
        pos += timestamp_fmt(modifs + pos, &p->limit) ;
        modifs[pos++] = 0 ;
      }
    }
    pathexec_r(argv, envp, env_len(envp), modifs, pos) ;
  }
  strerr_dieexec(111, argv[0]) ;
}
