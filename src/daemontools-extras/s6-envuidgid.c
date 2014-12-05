/* ISC license. */

#include <sys/types.h>
#include <pwd.h>
#include <limits.h>
#include <skalibs/gidstuff.h>
#include <skalibs/env.h>
#include <skalibs/strerr2.h>
#include <skalibs/fmtscan.h>
#include <skalibs/djbunix.h>

#define USAGE "s6-envuidgid username prog..."

int main (int argc, char const *const *argv, char const *const *envp)
{
  PROG = "s6-envuidgid" ;
  if (argc < 3) strerr_dieusage(100, USAGE) ;
  {
    char fmt[UINT64_FMT] ;
    struct passwd *pw = getpwnam(argv[1]) ;
    if (!pw) strerr_dief2x(1, "unknown user: ", argv[1]) ;
    fmt[gid_fmt(fmt, pw->pw_gid)] = 0 ;
    if (!pathexec_env("GID", fmt))
      strerr_diefu1sys(111, "update environment") ;
    fmt[uint64_fmt(fmt, pw->pw_uid)] = 0 ;
    if (!pathexec_env("UID", fmt))
      strerr_diefu1sys(111, "update environment") ;
  }
  
  {
    gid_t tab[NGROUPS_MAX] ;
    int n = prot_readgroups(argv[1], tab, NGROUPS_MAX) ;
    if (n < 0)
      strerr_diefu2sys(111, "get supplementary groups for ", argv[1]) ;
    {
      char fmt[GID_FMT * n] ;
      fmt[gid_fmtlist(fmt, tab, n)] = 0 ;
      if (!pathexec_env("GIDLIST", fmt))
        strerr_diefu1sys(111, "update environment") ;
    }
  }
  pathexec_fromenv(argv+2, envp, env_len(envp)) ;
  strerr_dieexec(111, argv[2]) ;
}
