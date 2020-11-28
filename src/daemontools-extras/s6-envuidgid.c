/* ISC license. */

#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <limits.h>

#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <skalibs/exec.h>

#define USAGE "s6-envuidgid [ -i | -D defaultuid:defaultgid ] [ -u | -g | -B ] [ -n ] account prog..."
#define dieusage() strerr_dieusage(100, USAGE)

static inline size_t scan_defaults (char const *s, uid_t *uid, gid_t *gid, size_t *n, gid_t *tab)
{
  size_t pos = uid_scan(s, uid) ;
  if (!pos)
  {
    if (*s != ':') return 0 ;
    *uid = 0 ;
  }
  s += pos ;
  if (!*s) goto zgid ;
  if (*s++ != ':') return 0 ;
  if (!*s) goto zgid ;
  pos = gid_scan(s, gid) ;
  if (!pos)
  {
    if (*s != ':') return 0 ;
    *gid = 0 ;
  }
  s += pos ;
  if (!*s) goto zn ;
  if (*s++ != ':') return 0 ;
  if (!*s) goto zn ;
  return gid_scanlist(tab, NGROUPS_MAX, s, n) ;

 zgid:
  *gid = 0 ;
 zn:
  *n = 0 ;
  return 1 ;
}

static int prot_readgroups (char const *name, gid_t *tab, unsigned int max)
{
  unsigned int n = 0 ;
  for (;;)
  {
    struct group *gr ;
    char **member ;
    errno = 0 ;
    if (n >= max) break ;
    gr = getgrent() ;
    if (!gr) break ;
    for (member = gr->gr_mem ; *member ; member++)
      if (!strcmp(name, *member)) break ;
    if (*member) tab[n++] = gr->gr_gid ;
  }
  endgrent() ;
  return errno ? -1 : n ;
}

int main (int argc, char *const *argv)
{
  char const *user = 0 ;
  char const *group = 0 ;
  unsigned int what = 7 ;
  int numfallback = 0 ;
  int insist = 1 ;
  uid_t uid ;
  gid_t gid ;
  size_t n ;
  gid_t tab[NGROUPS_MAX] ;
  PROG = "s6-envuidgid" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, (char const *const *)argv, "ugBniD:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'u' : what = 1 ; break ;
        case 'g' : what = 2 ; break ;
        case 'B' : what = 3 ; break ;
        case 'n' : what &= 3 ; numfallback = 1 ; break ;
        case 'i' : insist = 1 ; break ;
        case 'D' :
          if (!scan_defaults(l.arg, &uid, &gid, &n, tab)) dieusage() ;
          insist = 0 ;
          break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (argc < 2) dieusage() ;

  switch (what)
  {
    case 7 : /* account */
    case 1 : /* user */
      user = argv[0] ;
      break ;
    case 2 : /* group */
      group = argv[0] ;
      break ;
    case 3 : /* both */
    {
      char *colon = strchr(argv[0], ':') ;
      user = argv[0] ;
      if (colon)
      {
        *colon = 0 ;
        group = colon + 1 ;
        if (colon == argv[0]) user = 0 ;
        if (!group[0]) group = 0 ;
      }
      break ;
    }
    default : strerr_dief1x(101, "inconsistent option management - please submit a bug-report") ;
  }
  
  if (group)
  {
    struct group *gr = getgrnam(group) ;
    if (gr) gid = gr->gr_gid ;
    else if (numfallback && gid_scan(group, &gid)) ;
    else if (insist) strerr_dief2x(1, "unknown group: ", group) ;
  }

  if (user)
  {
    struct passwd *pw = getpwnam(user) ;
    if (pw)
    {
      uid = pw->pw_uid ;
      if (what == 7)
      {
        int r = prot_readgroups(argv[0], tab, NGROUPS_MAX) ;
        if (r < 0)
          strerr_diefu2sys(111, "get supplementary groups for ", argv[0]) ;
        n = r ;
        gid = pw->pw_gid ;
      }
    }
    else if (numfallback && uid_scan(user, &uid)) ;
    else if (insist) strerr_dief2x(1, "unknown user: ", user) ;
  }

  {
    size_t pos = 0 ;
    char fmt[19 + UID_FMT + (NGROUPS_MAX+1) * GID_FMT] ;
    if (what & 1)
    {
      memcpy(fmt + pos, "UID=", 4) ; pos += 4 ;
      pos += uid_fmt(fmt + pos, uid) ;
      fmt[pos++] = 0 ;
    }  
    if (what & 2)
    {
      memcpy(fmt + pos, "GID=", 4) ; pos += 4 ;
      pos += gid_fmt(fmt + pos, gid) ;
      fmt[pos++] = 0 ;
    }
    if (what & 4)
    {
      memcpy(fmt + pos, "GIDLIST=", 8) ; pos += 8 ;
      pos += gid_fmtlist(fmt + pos, tab, n) ;
      fmt[pos++] = 0 ;
    }
    xmexec_m((char const *const *)argv + 1, fmt, pos) ;
  }
}
