/* ISC license. */

#include <sys/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <s6/config.h>

#define USAGE "s6-setuidgid username prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char *const *argv, char const *const *envp)
{
  char const *newargv[argc + 7] ;
  size_t pos ;
  unsigned int m = 0 ;
  PROG = "s6-setuidgid" ;
  if (argc < 3) dieusage() ;
  argv++ ;
  pos = str_chr(argv[0], ':') ;
  if (argv[0][pos])
  {
    argv[0][pos] = 0 ;
    newargv[m++] = S6_BINPREFIX "s6-applyuidgid" ;
    newargv[m++] = "-u" ;
    newargv[m++] = argv[0] ;
    newargv[m++] = "-g" ;
    newargv[m++] = argv[0] + pos + 1 ;
    newargv[m++] = "-G" ;
    newargv[m++] = "" ;
    argv++ ;
  }
  else
  {
    newargv[m++] = S6_BINPREFIX "s6-envuidgid" ;
    newargv[m++] = *argv++ ;
    newargv[m++] = S6_BINPREFIX "s6-applyuidgid" ;
    newargv[m++] = "-Uz" ;
  }
  newargv[m++] = "--" ;
  while (*argv) newargv[m++] = *argv++ ;
  newargv[m++] = 0 ;
  pathexec_run(newargv[0], newargv, envp) ;
  strerr_dieexec(111, newargv[0]) ;
}
