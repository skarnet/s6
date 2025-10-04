/* ISC license. */

#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <skalibs/posixplz.h>
#include <skalibs/stat.h>
#include <skalibs/types.h>
#include <skalibs/cdbmake.h>
#include <skalibs/gol.h>
#include <skalibs/prog.h>
#include <skalibs/strerr.h>
#include <skalibs/stralloc.h>
#include <skalibs/env.h>
#include <skalibs/direntry.h>
#include <skalibs/djbunix.h>
#include <skalibs/skamisc.h>

#define USAGE "s6-accessrules-cdb-from-fs [ -m mode ] cdbfile dir"
#define SUFFIX ":s6-accessrules-cdb-from-fs:XXXXXX"

static stralloc tmp = STRALLOC_ZERO ;

static void cleanup (void)
{
  unlink_void(tmp.s) ;
}

static void dienomem (void)
{
  cleanup() ;
  strerr_diefu1sys(111, "stralloc_catb") ;
}

static void doit (cdbmaker *c, stralloc *sa, size_t start)
{
  size_t tmpbase = tmp.len ;
  unsigned int k = sa->len ;
  if (!stralloc_readyplus(sa, 10)) dienomem() ;
  stralloc_catb(sa, "/allow", 7) ;
  tmp.s[tmpbase] = 0 ;
  if (access(sa->s, F_OK) < 0)
  {
    if ((errno != ENOENT) && (errno != EACCES))
    {
      cleanup() ;
      strerr_diefu2sys(111, "access ", sa->s) ;
    }
    sa->len = k+1 ;
    stralloc_catb(sa, "deny", 5) ;
    if (access(sa->s, F_OK) < 0)
      if ((errno != ENOENT) && (errno != EACCES))
      {
        cleanup() ;
        strerr_diefu2sys(111, "access ", sa->s) ;
      }
      else return ;
    else if (!cdbmake_add(c, sa->s + start, k - start, "D", 1))
    {
      cleanup() ;
      strerr_diefu1sys(111, "cdbmake_add") ;
    }
  }
  else
  {
    uint16_t envlen = 0 ;
    uint16_t execlen = 0 ;
    ssize_t r ;
    tmp.s[tmpbase] = 'A' ;
    sa->len = k+1 ;
    stralloc_catb(sa, "env", 4) ;
    tmp.len = tmpbase + 3 ;
    if ((envdir(sa->s, &tmp) < 0) && (errno != ENOENT))
    {
      cleanup() ;
      strerr_diefu2sys(111, "envdir ", sa->s) ;
    }
    if (tmp.len > tmpbase + 4103)
    {
      cleanup() ;
      strerr_dief2sys(100, sa->s, " too big") ;
    }
    envlen = tmp.len - tmpbase - 3 ;
    tmp.len = tmpbase ;
    uint16_pack_big(tmp.s + tmpbase + 1, envlen) ;
    sa->len = k+1 ;
    stralloc_catb(sa, "exec", 5) ;
    r = openreadnclose(sa->s, tmp.s + tmpbase + 5 + envlen, 4096) ;
    if ((r == -1) && (errno != ENOENT))
    {
      cleanup() ;
      strerr_diefu2sys(111, "openreadnclose ", sa->s) ;
    }
    if (r > 0) execlen = r ;
    if (execlen == 4096) strerr_warnw2x("possibly truncated file ", sa->s) ;
    uint16_pack_big(tmp.s + tmpbase + 3 + envlen, execlen) ;
    if (!cdbmake_add(c, sa->s + start, k - start, tmp.s + tmpbase, 5 + envlen + execlen))
    {
      cleanup() ;
      strerr_diefu1sys(111, "cdbmake_add") ;
    }
  }
}

enum gola_e
{
  GOLA_MODE,
  GOLA_N
} ;

static gol_arg const rgola[] =
{
  { .so = 'm', .lo = "mode", .i = GOLA_MODE }
} ;

int main (int argc, char const *const *argv)
{
  stralloc sa = STRALLOC_ZERO ;
  cdbmaker c = CDBMAKER_ZERO ;
  DIR *dir ;
  size_t start ;
  int fd ;
  unsigned int mode ;
  char const *wgola[GOLA_N] = { 0 } ;
  unsigned int golc ;
  PROG = "s6-accessrules-cdb-from-fs" ;

  golc = gol_main(argc, argv, 0, 0, rgola, 1, 0, wgola) ;
  argc -= golc ; argv += golc ;
  if (argc < 3) strerr_dieusage(100, USAGE) ;

  if (wgola[GOLA_MODE] && !uint0_oscan(wgola[GOLA_MODE], &mode))
    strerr_dief1x(100, "mode needs to be an unsigned integer (in octal)") ;
  if (!stralloc_cats(&tmp, argv[1])) return 0 ;
  if (!stralloc_readyplus(&tmp, 8210))
    strerr_diefu1sys(111, "stralloc_catb") ;
  stralloc_catb(&tmp, SUFFIX, sizeof(SUFFIX)) ;
  fd = mkstemp(tmp.s) ;
  if (fd < 0) strerr_diefu2sys(111, "mkstemp ", tmp.s) ;
  if (!cdbmake_start(&c, fd))
  {
    cleanup() ;
    strerr_diefu1sys(111, "cdbmake_start") ;
  }
  dir = opendir(argv[2]) ;
  if (!dir)
  {
    cleanup() ;
    strerr_diefu2sys(111, "opendir ", argv[2]) ;
  }
  if (!stralloc_cats(&sa, argv[2]) || !stralloc_catb(&sa, "/", 1)) dienomem() ;
  start = sa.len ;

  for (;;)
  {
    DIR *subdir ;
    direntry *d ;
    size_t base ;
    errno = 0 ;
    d = readdir(dir) ;
    if (!d) break ;
    if (d->d_name[0] == '.') continue ;
    sa.len = start ;
    if (!stralloc_cats(&sa, d->d_name) || !stralloc_0(&sa)) dienomem() ;
    base = sa.len ;
    subdir = opendir(sa.s) ;
    if (!subdir)
    {
      cleanup() ;
      strerr_diefu2sys(111, "opendir ", sa.s) ;
    }
    sa.s[base-1] = '/' ;
    for (;;)
    {
      errno = 0 ;
      d = readdir(subdir) ;
      if (!d) break ;
      if (d->d_name[0] == '.') continue ;
      sa.len = base ;
      if (!stralloc_cats(&sa, d->d_name)) dienomem() ;
      doit(&c, &sa, start) ;
    }
    if (errno)
    {
      sa.s[base-1] = 0 ;
      cleanup() ;
      strerr_diefu2sys(111, "readdir ", sa.s) ;
    }
    dir_close(subdir) ;
  }
  if (errno)
  {
    cleanup() ;
    strerr_diefu2sys(111, "readdir ", argv[2]) ;
  }
  dir_close(dir) ;
  if (!cdbmake_finish(&c))
  {
    cleanup() ;
    strerr_diefu1sys(111, "cdb_make_finish") ;
  }
  if (fd_sync(fd) < 0)
  {
    cleanup() ;
    strerr_diefu1sys(111, "fd_sync") ;
  }
  fd_close(fd) ;
  if (wgola[GOLA_MODE] && chmod(tmp.s, mode) == -1)
  {
    cleanup() ;
    strerr_diefu2sys(111, "chmod ", tmp.s) ;
  }
  if (rename(tmp.s, argv[1]) < 0)
  {
    cleanup() ;
    strerr_diefu4sys(111, "rename ", tmp.s, " to ", argv[1]) ;
  }
  return 0 ;
}
