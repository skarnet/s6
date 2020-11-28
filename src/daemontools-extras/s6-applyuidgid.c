/* ISC license. */

#include <unistd.h>
#include <grp.h>
#include <limits.h>
#include <stdlib.h>

#include <skalibs/types.h>
#include <skalibs/setgroups.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/djbunix.h>
#include <skalibs/exec.h>

#define USAGE "s6-applyuidgid [ -z ] [ -u uid ] [ -g gid ] [ -G gidlist ] [ -U ] prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  uid_t uid = 0 ;
  gid_t gid = 0 ;
  gid_t gids[NGROUPS_MAX+1] ;
  size_t gidn = (size_t)-1 ;
  int unexport = 0 ;
  PROG = "s6-applyuidgid" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "zUu:g:G:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'z' : unexport = 1 ; break ;
        case 'u' : if (!uid0_scan(l.arg, &uid)) dieusage() ; break ;
        case 'g' : if (!gid0_scan(l.arg, &gid)) dieusage() ; break ;
        case 'G' : if (!gid_scanlist(gids, NGROUPS_MAX, l.arg, &gidn) && *l.arg) dieusage() ; break ;
        case 'U' :
        {
          char const *x = getenv("UID") ;
          if (!x) strerr_dienotset(100, "UID") ;
          if (!uid0_scan(x, &uid)) strerr_dieinvalid(100, "UID") ;
          x = getenv("GID") ;
          if (!x) strerr_dienotset(100, "GID") ;
          if (!gid0_scan(x, &gid)) strerr_dieinvalid(100, "GID") ;
          x = getenv("GIDLIST") ;
          if (!x) strerr_dienotset(100, "GIDLIST") ;
          if (!gid_scanlist(gids, NGROUPS_MAX+1, x, &gidn) && *x)
            strerr_dieinvalid(100, "GIDLIST") ;
          break ;
        }
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) dieusage() ;

  if (gidn != (size_t)-1 && setgroups_and_gid(gid ? gid : getegid(), gidn, gids) < 0)
    strerr_diefu1sys(111, "set supplementary group list") ;
  if (gid && setgid(gid) < 0)
    strerr_diefu1sys(111, "setgid") ;
  if (uid && setuid(uid) < 0)
    strerr_diefu1sys(111, "setuid") ;

  if (unexport) xmexec_n(argv, "UID\0GID\0GIDLIST", 16, 3) ;
  else xexec(argv) ;
}
