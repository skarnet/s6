/* ISC license. */

#include <sys/types.h>
#include <pwd.h>
#include <limits.h>
#include <skalibs/uint64.h>
#include <skalibs/gidstuff.h>
#include <skalibs/bytestr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/env.h>
#include <skalibs/fmtscan.h>
#include <skalibs/djbunix.h>

#define USAGE "s6-envuidgid [ -i | -D defaultuid:defaultgid ] username prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  struct passwd *pw ;
  uint64 uid ;
  gid_t gid ;
  gid_t tab[NGROUPS_MAX] ;
  int n = 0 ;
  int insist = 1 ;
  PROG = "s6-envuidgid" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "iD:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'i' : insist = 1 ; break ;
        case 'D' :
        {
          unsigned int pos = uint64_scan(l.arg, &uid) ;
          if (!pos)
          {
            if (l.arg[pos] != ':') dieusage() ;
            uid = 0 ;
          }
          if (!l.arg[pos]) gid = 0 ;
          else
          {
            if (l.arg[pos++] != ':') dieusage() ;
            if (!l.arg[pos]) gid = 0 ;
            else if (!gid0_scan(l.arg + pos, &gid)) dieusage() ;
          }
          insist = 0 ;
          break ;
        }
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (argc < 2) dieusage() ;

  pw = getpwnam(argv[0]) ;
  if (pw)
  {
    uid = pw->pw_uid ;
    gid = pw->pw_gid ;
    n = prot_readgroups(argv[0], tab, NGROUPS_MAX) ;
    if (n < 0)
      strerr_diefu2sys(111, "get supplementary groups for ", argv[0]) ;
  }
  else if (insist) strerr_dief2x(1, "unknown user: ", argv[0]) ;

  {
    unsigned int pos = 0 ;
    char fmt[19 + UINT64_FMT + (n+1) * GID_FMT] ;
    byte_copy(fmt + pos, 4, "UID=") ; pos += 4 ;
    pos += uint64_fmt(fmt + pos, uid) ;
    byte_copy(fmt + pos, 5, "\0GID=") ; pos += 5 ;
    pos += gid_fmt(fmt + pos, gid) ;
    byte_copy(fmt + pos, 9, "\0GIDLIST=") ; pos += 9 ;
    pos += gid_fmtlist(fmt + pos, tab, n) ;
    fmt[pos++] = 0 ;
    pathexec_r(argv+1, envp, env_len(envp), fmt, pos) ;
  }
  strerr_dieexec(111, argv[1]) ;
}
