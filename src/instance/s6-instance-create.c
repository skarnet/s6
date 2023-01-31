/* ISC license. */

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include <skalibs/bytestr.h>
#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/strerr.h>
#include <skalibs/djbunix.h>

#include <s6/supervise.h>

#define USAGE "s6-instance-create [ -d | -D ] [ -f ] [ -P ] [ -t timeout ] service instancename"
#define dieusage() strerr_dieusage(100, USAGE)

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
  if (!argv[1][0] || argv[1][0] == '.' || byte_in(argv[1], namelen, " \t\f\r\n", 5) < namelen)
    strerr_dief1x(100, "invalid instance name") ;

  {
    mode_t m = umask(0) ;
    size_t svlen = strlen(argv[0]) ;
    char fn[svlen + 11] ;
    memcpy(fn, argv[0], svlen) ;
    memcpy(fn + svlen, "/instances", 11) ;
    if (mkdir(fn, 0755) == -1 && errno != EEXIST)
      strerr_diefu2sys(111, "mkdir ", fn) ;
    fn[svlen + 9] = 0 ;
    if (mkdir(fn, 0755) == -1 && errno != EEXIST)
      strerr_diefu2sys(111, "mkdir ", fn) ;
    umask(m) ;
  }

  s6_instance_chdirservice(argv[0]) ;

  tain_now_set_stopwatch_g() ;
  tain_add_g(&tto, &tto) ;

  {
    char sv[namelen + 14] ;
    char const *p = sv ;
    memcpy(sv, "../instances/", 13) ;
    memcpy(sv + 13, argv[1], namelen + 1) ;
    {
      struct stat st ;
      if (stat(sv, &st) == 0)
        if (!S_ISDIR(st.st_mode))
          strerr_dief3x(100, "unexpected file preventing instance creation at ", argv[0], sv+2) ;
        else
          strerr_dief3x(100, "instance appears to already exist at ", argv[0], sv+2) ;
      else if (errno != ENOENT)
        strerr_diefu3sys(111, "stat ", argv[0], sv+2) ;
    }
    if (!hiercopy("../template", sv))
    {
      cleanup(sv) ;
      strerr_diefu5sys(111, "copy ", argv[0], "/template to ", argv[0], sv+2) ;
    }
    if (s6_supervise_link_names_g(".", &p, argv + 1, 1, options, &tto) == -1)
    {
      cleanup(sv) ;
      strerr_diefu4sys(errno == ETIMEDOUT ? 99 : 111, "create instance of ", argv[0], " named ", argv[1]) ;
    }
  }
  return 0 ;
}
