/* ISC license. */

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>

#include <s6/s6-supervise.h>

#define USAGE "s6-svdir-link [ -d ] [ -f ] [ -P | -p ] [ -t timeout ] scandir name servicedir"
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
  memcpy(fn + len, "/run", 4) ;
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
  PROG = "s6-svdir-link" ;
  {
    unsigned int t = 0 ;
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "dfPpt:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'd' : options |= 4 ; break ;
        case 'f' : options |= 1 ; break ;
        case 'P' : options |= 2 ; break ;
        case 'p' : options &= ~2U ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&tto, t) ;
  }
  if (argc < 3) dieusage() ;

  if (!argv[0][0]) strerr_dief1x(100, "invalid scandir") ;
  if (!argv[1][0] || argv[1][0] == '.' || argv[1][0] == '/')
    strerr_dief1x(100, "invalid name") ;
  if (!argv[2][0]) strerr_dief1x(100, "invalid servicedir") ;
  checkscandir(argv[0]) ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&tto, &tto) ;

  if (s6_supervise_link_names_g(argv[0], argv + 2, argv + 1, 1, options, &tto) == -1)
    strerr_diefu6sys(111, "link servicedir ", argv[2], " into scandir ", argv[0], " with name ", argv[1]) ;
  return 0 ;
}
