/* ISC license. */

#include <string.h>

#include <skalibs/strerr2.h>
#include <skalibs/exec.h>

#include <s6/config.h>

#define USAGE "s6-setuidgid username prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char *const *argv)
{
  char const *newargv[argc + 7] ;
  char *colon ;
  unsigned int m = 0 ;
  PROG = "s6-setuidgid" ;
  if (argc < 3) dieusage() ;
  argv++ ;
  colon = strchr(argv[0], ':') ;
  if (colon)
  {
    *colon = 0 ;
    newargv[m++] = S6_BINPREFIX "s6-applyuidgid" ;
    newargv[m++] = "-u" ;
    newargv[m++] = argv[0] ;
    newargv[m++] = "-g" ;
    newargv[m++] = colon + 1 ;
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
  xexec(newargv) ;
}
