/* ISC license. */

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <skalibs/bytestr.h>
#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/strerr.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <s6/supervise.h>

#define USAGE "s6-instance-create [ -d | -D ] [ -f ] [ -P ] [ -t timeout ] service instancename"
#define dieusage() strerr_dieusage(100, USAGE)

static inline void checkinstanced (char const *s)  /* chdirs */
{
  int fd, r ;
  size_t len = strlen(s) ;
  char fn[len + 10] ;
  memcpy(fn, s, len) ;
  memcpy(fn + len, "/instance", 10) ;
  if (chdir(fn) == -1) strerr_diefu2sys(111, "chdir to ", fn) ;
  fd = open_read(S6_SVSCAN_CTLDIR "/lock") ;
  if (fd < 0) strerr_diefu3sys(111, "open ", fn, "/" S6_SVSCAN_CTLDIR "/lock") ;
  r = fd_islocked(fd) ;
  if (r < 0) strerr_diefu3sys(111, "check lock on ", fn, "/" S6_SVSCAN_CTLDIR "/lock") ;
  if (!r) strerr_dief2x(1, "instanced service not running on ", s) ;
  fd_close(fd) ;
}

static void cleanup (char const *s)
{
  int e = errno ;
  rm_rf(s) ;
  errno = e ;
}

int main (int argc, char const *const *argv)
{
  tain tto = TAIN_INFINITE_RELATIVE ;
  size_t namelen ;
  uint32_t options = 16 ;
  PROG = "s6-instance-create" ;
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

  namelen = strlen(argv[1]) ;
  if (!argv[0][0]) strerr_dief1x(100, "invalid service path") ;
  if (!argv[1][0] || argv[1][0] == '.' || byte_in(argv[1], namelen, " \t\f\r\n", 5) < 5)
    strerr_dief1x(100, "invalid instance name") ;
  checkinstanced(argv[0]) ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&tto, &tto) ;

  {
    char sv[namelen + 14] ;
    memcpy(sv, "../instances/", 13) ;
    memcpy(sv + 13, argv[1], namelen + 1) ;
    if (!hiercopy("../instances/.template", sv))
    {
      cleanup(sv) ;
      strerr_diefu5sys(111, "copy ", argv[0], "/instances/.template to ", argv[0], sv+2) ;
    }
    if (s6_supervise_link_names_g(".", (char const *const *)&sv, argv + 1, 1, options, &tto) == -1)
    {
      cleanup(sv) ;
      strerr_diefu4sys(errno == ETIMEDOUT ? 99 : 111, "creatre instance of ", argv[0], " named ", argv[1]) ;
    }
  }
  return 0 ;
}
