/* ISC license. */

#include <string.h>

#include <skalibs/bytestr.h>
#include <skalibs/strerr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/exec.h>

#include <s6/config.h>

#define USAGE "s6-instance-status [ -uwNrpest | -o up,wantedup,normallyup,ready,paused,pid,exitcode,signal,signum,updownsince,readysince,updownfor,readyfor ] [ -n ] service name"
#define dieusage() strerr_dieusage(100, USAGE)

#define MAXFIELDS 16
#define checkfields() if (n++ >= MAXFIELDS) strerr_dief1x(100, "too many option fields")

static unsigned int check_options (char const *arg, unsigned int n)
{
  static char const *table[] =
  {
    "up",
    "wantedup",
    "normallyup",
    "ready",
    "paused",
    "pid",
    "exitcode",
    "signal",
    "signum",
    "updownsince",
    "readysince",
    "updownfor",
    "readyfor",
    0
  } ;
  while (*arg)
  {
    size_t pos = str_chr(arg, ',') ;
    char const *const *p = table ;
    if (!pos) strerr_dief1x(100, "invalid null option field") ;
    for (; *p ; p++) if (!strncmp(arg, *p, pos)) break ;
    if (!p)
    {
      char blah[pos+1] ;
      memcpy(blah, arg, pos) ;
      blah[pos] = 0 ;
      strerr_dief2x(100, "invalid option field: ", blah) ;
    }
    checkfields() ;
    arg += pos ; if (*arg) arg++ ;
  }
  return n ;
}

int main (int argc, char const **argv)
{
  char const **fullargv = argv ;
  size_t namelen ;
  unsigned int n = 0 ;
  PROG = "s6-instance-status" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "no:uwNrpest", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'n' : break ;
        case 'o' : n = check_options(l.arg, n) ; break ;
        case 'u' :
        case 'w' :
        case 'N' :
        case 'r' :
        case 'p' :
        case 'e' :
        case 's' :
        case 't' : if (n++ >= MAXFIELDS) strerr_dief1x(100, "too many option fields") ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (argc < 2) dieusage() ;
  namelen = strlen(argv[1]) ;
  if (!argv[0][0]) strerr_dief1x(100, "invalid service name") ;
  if (!argv[1][0] || argv[1][0] == '.' || byte_in(argv[1], namelen, " \t\f\r\n", 5) < namelen)
    strerr_dief1x(100, "invalid instance name") ;

  {
    size_t svlen = strlen(argv[0]) ;
    char fn[svlen + 11 + namelen] ;
    memcpy(fn, argv[0], svlen) ;
    memcpy(fn + svlen, "/instance/", 10) ;
    memcpy(fn + svlen + 10, argv[1], namelen + 1) ;
    argv[0] = fn ;
    argv[1] = 0 ;
    fullargv[0] = S6_BINPREFIX "s6-svstat" ;
    xexec(fullargv) ;
  }
}
