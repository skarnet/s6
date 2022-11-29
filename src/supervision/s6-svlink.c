/* ISC license. */

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>

#include <skalibs/posixplz.h>
#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/strerr.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <s6/supervise.h>

#define USAGE "s6-svlink [ -d | -D ] [ -f ] [ -P ] [ -t timeout ] scandir servicedir [ name ]"
#define dieusage() strerr_dieusage(100, USAGE)

static inline void checkscandir (char const *s)
{
  int r ;
  int fd ;
  size_t len = strlen(s) ;
  char fn[len + 6 + sizeof(S6_SVSCAN_CTLDIR)] ;
  memcpy(fn, s, len) ;
  memcpy(fn + len, "/" S6_SVSCAN_CTLDIR "/lock", 6 + sizeof(S6_SVSCAN_CTLDIR)) ;
  fd = open_read(fn) ;
  if (fd < 0) strerr_diefu2sys(111, "open ", fn) ;
  r = fd_islocked(fd) ;
  if (r < 0) strerr_diefu2sys(111, "check lock on ", fn) ;
  if (!r) strerr_dief2x(1, "s6-svscan not running on ", s) ;
  fd_close(fd) ;
}

static inline void checkservicedir (char const *s)
{
  int r ;
  struct stat st ;
  size_t len = strlen(s) ;
  char fn[len + 9] ;
  memcpy(fn, s, len) ;
  memcpy(fn + len, "/run", 5) ;
  if (stat(fn, &st) == -1) strerr_diefu2sys(111, "stat ", fn) ;
  if (!(st.st_mode & S_IXUSR)) strerr_dief2x(100, fn, " is not executable") ;
  r = s6_svc_ok(s) ;
  if (r < 0) strerr_diefu2sys(111, "check supervision status of ", s) ;
  if (r) strerr_warnw2x("supervisor already running on ", s) ;
  memcpy(fn + len + 1, "log", 4) ;
  if (stat(fn, &st) == -1)
  {
    if (errno != ENOENT) strerr_diefu2sys(111, "stat ", fn) ;
  }
  else
  {
    if (!S_ISDIR(st.st_mode)) strerr_dief2x(100, fn, " is not a directory") ;
    memcpy(fn + len + 4, "/run", 5) ;
    if (stat(fn, &st) == -1) strerr_diefu2sys(111, "stat ", fn) ;
    if (!(st.st_mode & S_IXUSR)) strerr_dief2x(100, fn, " is not executable") ;
    fn[len + 4] = 0 ;
    r = s6_svc_ok(fn) ;
    if (r < 0) strerr_diefu2sys(111, "check supervision status of ", fn) ;
    if (r) strerr_warnw2x("supervisor already running on ", fn) ;
  } 
}

int main (int argc, char const *const *argv)
{
  tain tto = TAIN_INFINITE_RELATIVE ;
  uint32_t options = 0 ;
  char const *name ;
  PROG = "s6-svlink" ;
  {
    unsigned int t = 0 ;
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "dDfPt:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'd' : options |= 12 ; break ;
        case 'D' : options |= 4 ; options &= ~8U ; break ;
        case 'f' : options |= 1 ; break ;
        case 'P' : options |= 2 ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&tto, t) ;
  }
  if (argc < 2) dieusage() ;

  if (argv[2]) name = argv[2] ;
  else
  {
    stralloc sa = STRALLOC_ZERO ;
    if (!stralloc_cats(&sa, argv[1]) || !stralloc_0(&sa))
      strerr_diefu1sys(111, "stralloc_cats") ;
    name = basename(sa.s) ;
  }
  if (!argv[0][0]) strerr_dief1x(100, "invalid scandir") ;
  if (!argv[1][0]) strerr_dief1x(100, "invalid servicedir") ;
  if (!name[0] || name[0] == '.' || name[0] == '/')
    strerr_dief1x(100, "invalid name") ;
  checkscandir(argv[0]) ;
  checkservicedir(argv[1]) ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&tto, &tto) ;

  if (s6_supervise_link_names_g(argv[0], argv + 1, &name, 1, options, &tto) == -1)
    strerr_diefu6sys(errno == ETIMEDOUT ? 99 : 111, "link servicedir ", argv[1], " into scandir ", argv[0], " with name ", name) ;
  return 0 ;
}
