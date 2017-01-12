/* ISC license. */

#include <skalibs/nonposix.h>
#include <sys/types.h>
#include <unistd.h>
#include <grp.h>
#include <limits.h>
#include <skalibs/uint64.h>
#include <skalibs/gidstuff.h>
#include <skalibs/setgroups.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/env.h>
#include <skalibs/djbunix.h>

#define USAGE "s6-applyuidgid [ -z ] [ -u uid ] [ -g gid ] [ -G gidlist ] [ -U ] prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  uint64 uid = 0 ;
  gid_t gid = 0 ;
  gid_t gids[NGROUPS_MAX] ;
  unsigned int gidn = (unsigned int)-1 ;
  int unexport = 0 ;
  PROG = "s6-applyuidgid" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "zUu:g:G:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'z' : unexport = 1 ; break ;
        case 'u' : if (!uint640_scan(l.arg, &uid)) dieusage() ; break ;
        case 'g' : if (!gid0_scan(l.arg, &gid)) dieusage() ; break ;
        case 'G' : if (!gid_scanlist(gids, NGROUPS_MAX, l.arg, &gidn) && *l.arg) dieusage() ; break ;
        case 'U' :
        {
          char const *x = env_get2(envp, "UID") ;
          if (!x) strerr_dienotset(100, "UID") ;
          if (!uint640_scan(x, &uid)) strerr_dieinvalid(100, "UID") ;
          x = env_get2(envp, "GID") ;
          if (!x) strerr_dienotset(100, "GID") ;
          if (!gid0_scan(x, &gid)) strerr_dieinvalid(100, "GID") ;
          x = env_get2(envp, "GIDLIST") ;
          if (!x) strerr_dienotset(100, "GIDLIST") ;
          if (!gid_scanlist(gids, NGROUPS_MAX, x, &gidn) && *x)
            strerr_dieinvalid(100, "GIDLIST") ;
          break ;
        }
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) dieusage() ;

  if (uid > (uint64)(uid_t)-1) strerr_dief1x(100, "uid value too big") ;
  if (gidn != (unsigned int)-1 && setgroups(gidn, gids) < 0)
    strerr_diefu1sys(111, "set supplementary group list") ;
  if (gid && setgid(gid) < 0)
    strerr_diefu1sys(111, "setgid") ;
  if (uid && setuid((uid_t)uid) < 0)
    strerr_diefu1sys(111, "setuid") ;

  if (unexport) pathexec_r(argv, envp, env_len(envp), "UID\0GID\0GIDLIST", 16) ;
  else pathexec_run(argv[0], argv, envp) ;
  strerr_dieexec(111, argv[0]) ;
}
