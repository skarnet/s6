/* ISC license. */

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <limits.h>
#include <skalibs/uint64.h>
#include <skalibs/gidstuff.h>
#include <skalibs/bytestr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/env.h>
#include <skalibs/fmtscan.h>
#include <skalibs/djbunix.h>

#define USAGE "s6-envuidgid [ -i | -D defaultuid:defaultgid ] [ -u | -g | -B ] [ -n ] account prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char *const *argv, char const *const *envp)
{
  char const *user = 0 ;
  char const *group = 0 ;
  int what = 0 ;
  int numfallback = 0 ;
  int insist = 1 ;
  unsigned int pos ;
  uint64 uid ;
  gid_t gid ;
  char fmt[19 + UINT64_FMT + (NGROUPS_MAX+1) * GID_FMT] ;
  PROG = "s6-envuidgid" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, (char const *const *)argv, "ugBniD:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'u' : what = 1 ; break ;
        case 'g' : what = 2 ; break ;
        case 'B' : what = 3 ; break ;
        case 'n' : what = 3 ; numfallback = 1 ; break ;
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
          what = 3 ;
          break ;
        }
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (argc < 2) dieusage() ;

  switch (what)
  {
    case 0 : /* account */
    case 1 : /* user */
      user = argv[0] ;
      break ;
    case 2 : /* group */
      group = argv[0] ;
      break ;
    case 3 : /* both */
      user = argv[0] ;
      pos = str_chr(argv[0], ':') ;
      if (argv[0][pos])
      {
        argv[0][pos] = 0 ;
        group = argv[0] + pos + 1 ;
        if (!pos) user = 0 ;
      }
      break ;
    default : strerr_dief1x(101, "inconsistent option management - please submit a bug-report") ;
  }
  
  pos = 0 ;

  if (group)
  {
    struct group *gr = getgrnam(group) ;
    if (gr) gid = gr->gr_gid ;
    else if (numfallback && gid_scan(group, &gid)) ;
    else if (insist) strerr_dief2x(1, "unknown group: ", group) ;
    byte_copy(fmt + pos, 4, "GID=") ; pos += 4 ;
    pos += gid_fmt(fmt + pos, gid) ;
    fmt[pos++] = 0 ;
  }

  if (user)
  {
    struct passwd *pw = getpwnam(user) ;
    if (pw)
    {
      uid = pw->pw_uid ;
      if (!what)
      {
        gid_t tab[NGROUPS_MAX] ;
        int n = prot_readgroups(argv[0], tab, NGROUPS_MAX) ;
        if (n < 0)
          strerr_diefu2sys(111, "get supplementary groups for ", argv[0]) ;
        byte_copy(fmt + pos, 4, "GID=") ; pos += 4 ;
        pos += gid_fmt(fmt + pos, pw->pw_gid) ;
        byte_copy(fmt + pos, 9, "\0GIDLIST=") ; pos += 9 ;
        pos += gid_fmtlist(fmt + pos, tab, n) ;
        fmt[pos++] = 0 ;
      }
    }
    else if (numfallback && uint64_scan(user, &uid)) ;
    else if (insist) strerr_dief2x(1, "unknown user: ", user) ;
    byte_copy(fmt + pos, 4, "UID=") ; pos += 4 ;
    pos += uint64_fmt(fmt + pos, uid) ;
    fmt[pos++] = 0 ;
  }

  pathexec_r((char const *const *)argv + 1, envp, env_len(envp), fmt, pos) ;
  strerr_dieexec(111, argv[1]) ;
}
