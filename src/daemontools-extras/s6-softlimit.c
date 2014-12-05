/* ISC license. */

#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/uint64.h>
#include <skalibs/djbunix.h>

#define USAGE "s6-softlimit [ -a allbytes ] [ -c corebytes ] [ -d databytes ] [ -f filebytes ] [ -l lockbytes ] [ -m membytes ] [ -o openfiles ] [ -p processes ] [ -r residentbytes ] [ -s stackbytes ] [ -t cpusecs ] prog..."

static void doit (int res, char const *arg)
{
  struct rlimit r ;
  if (getrlimit(res, &r) < 0) strerr_diefu1sys(111, "getrlimit") ;
  if ((arg[0] == '=') && !arg[1]) r.rlim_cur = r.rlim_max ;
  else
  {
    uint64 n ;
    if (!uint640_scan(arg, &n)) strerr_dieusage(100, USAGE) ;
    if (n > (uint64)r.rlim_max) n = (uint64)r.rlim_max ;
    r.rlim_cur = (rlim_t)n ;
  }
  if (setrlimit(res, &r) == -1) strerr_diefu1sys(111, "setrlimit") ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{
  PROG = "s6-softlimit" ;
  for (;;)
  {
    register int opt = sgetopt(argc, argv, "a:c:d:f:l:m:o:p:r:s:t:") ;
    if (opt == -1) break ;
    switch (opt)
    {
      case 'a' :
#ifdef RLIMIT_AS
        doit(RLIMIT_AS, subgetopt_here.arg) ;
#endif
#ifdef RLIMIT_VMEM
        doit(RLIMIT_VMEM, subgetopt_here.arg) ;
#endif
        break ;
      case 'c' :
#ifdef RLIMIT_CORE
        doit(RLIMIT_CORE, subgetopt_here.arg) ;
#endif
        break ;
      case 'd' :
#ifdef RLIMIT_DATA
        doit(RLIMIT_DATA, subgetopt_here.arg) ;
#endif
        break ;
      case 'f' :
#ifdef RLIMIT_FSIZE
        doit(RLIMIT_FSIZE, subgetopt_here.arg) ;
#endif
        break ;
      case 'l' :
#ifdef RLIMIT_MEMLOCK
        doit(RLIMIT_MEMLOCK, subgetopt_here.arg) ;
#endif
        break ;
      case 'm' :
#ifdef RLIMIT_DATA
        doit(RLIMIT_DATA, subgetopt_here.arg) ;
#endif
#ifdef RLIMIT_STACK
        doit(RLIMIT_STACK, subgetopt_here.arg) ;
#endif
#ifdef RLIMIT_MEMLOCK
        doit(RLIMIT_MEMLOCK, subgetopt_here.arg) ;
#endif
#ifdef RLIMIT_VMEM
        doit(RLIMIT_VMEM, subgetopt_here.arg) ;
#endif
#ifdef RLIMIT_AS
        doit(RLIMIT_AS, subgetopt_here.arg) ;
#endif
	break ;
      case 'o' :
#ifdef RLIMIT_NOFILE
        doit(RLIMIT_NOFILE, subgetopt_here.arg) ;
#endif
#ifdef RLIMIT_OFILE
        doit(RLIMIT_OFILE, subgetopt_here.arg) ;
#endif
        break ;
      case 'p' :
#ifdef RLIMIT_NPROC
        doit(RLIMIT_NPROC, subgetopt_here.arg) ;
#endif
        break ;
      case 'r' :
#ifdef RLIMIT_RSS
        doit(RLIMIT_RSS, subgetopt_here.arg) ;
#endif
        break ;
      case 's' :
#ifdef RLIMIT_STACK
        doit(RLIMIT_STACK, subgetopt_here.arg) ;
#endif
        break ;
      case 't' :
#ifdef RLIMIT_CPU
        doit(RLIMIT_CPU, subgetopt_here.arg) ;
#endif
        break ;
    }
  }
  argc -= subgetopt_here.ind ; argv += subgetopt_here.ind ;
  if (!argc) strerr_dieusage(100, USAGE) ;
  pathexec_run(argv[0], argv, envp) ;
  strerr_dieexec(111, argv[0]) ;
}
