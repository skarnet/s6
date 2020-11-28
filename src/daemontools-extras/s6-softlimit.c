/* ISC license. */

#include <sys/types.h>
#include <sys/resource.h>

#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/uint64.h>
#include <skalibs/exec.h>

#define USAGE "s6-softlimit [ -a allbytes ] [ -c corebytes ] [ -d databytes ] [ -f filebytes ] [ -l lockbytes ] [ -m membytes ] [ -o openfiles ] [ -p processes ] [ -r residentbytes ] [ -s stackbytes ] [ -t cpusecs ] prog..."

static void doit (int res, char const *arg)
{
  struct rlimit r ;
  if (getrlimit(res, &r) < 0) strerr_diefu1sys(111, "getrlimit") ;
  if ((arg[0] == '=') && !arg[1]) r.rlim_cur = r.rlim_max ;
  else
  {
    uint64_t n ;
    if (!uint640_scan(arg, &n)) strerr_dieusage(100, USAGE) ;
    if (n > (uint64_t)r.rlim_max) n = (uint64_t)r.rlim_max ;
    r.rlim_cur = (rlim_t)n ;
  }
  if (setrlimit(res, &r) < 0) strerr_diefu1sys(111, "setrlimit") ;
}

int main (int argc, char const *const *argv)
{
  subgetopt_t l = SUBGETOPT_ZERO ;
  PROG = "s6-softlimit" ;
  for (;;)
  {
    int opt = subgetopt_r(argc, argv, "a:c:d:f:l:m:o:p:r:s:t:", &l) ;
    if (opt == -1) break ;
    switch (opt)
    {
      case 'a' :
#ifdef RLIMIT_AS
        doit(RLIMIT_AS, l.arg) ;
#endif
#ifdef RLIMIT_VMEM
        doit(RLIMIT_VMEM, l.arg) ;
#endif
        break ;
      case 'c' :
#ifdef RLIMIT_CORE
        doit(RLIMIT_CORE, l.arg) ;
#endif
        break ;
      case 'd' :
#ifdef RLIMIT_DATA
        doit(RLIMIT_DATA, l.arg) ;
#endif
        break ;
      case 'f' :
#ifdef RLIMIT_FSIZE
        doit(RLIMIT_FSIZE, l.arg) ;
#endif
        break ;
      case 'l' :
#ifdef RLIMIT_MEMLOCK
        doit(RLIMIT_MEMLOCK, l.arg) ;
#endif
        break ;
      case 'm' :
#ifdef RLIMIT_DATA
        doit(RLIMIT_DATA, l.arg) ;
#endif
#ifdef RLIMIT_STACK
        doit(RLIMIT_STACK, l.arg) ;
#endif
#ifdef RLIMIT_MEMLOCK
        doit(RLIMIT_MEMLOCK, l.arg) ;
#endif
#ifdef RLIMIT_VMEM
        doit(RLIMIT_VMEM, l.arg) ;
#endif
#ifdef RLIMIT_AS
        doit(RLIMIT_AS, l.arg) ;
#endif
	break ;
      case 'o' :
#ifdef RLIMIT_NOFILE
        doit(RLIMIT_NOFILE, l.arg) ;
#endif
#ifdef RLIMIT_OFILE
        doit(RLIMIT_OFILE, l.arg) ;
#endif
        break ;
      case 'p' :
#ifdef RLIMIT_NPROC
        doit(RLIMIT_NPROC, l.arg) ;
#endif
        break ;
      case 'r' :
#ifdef RLIMIT_RSS
        doit(RLIMIT_RSS, l.arg) ;
#endif
        break ;
      case 's' :
#ifdef RLIMIT_STACK
        doit(RLIMIT_STACK, l.arg) ;
#endif
        break ;
      case 't' :
#ifdef RLIMIT_CPU
        doit(RLIMIT_CPU, l.arg) ;
#endif
        break ;
    }
  }
  argc -= l.ind ; argv += l.ind ;
  if (!argc) strerr_dieusage(100, USAGE) ;
  xexec(argv) ;
}
