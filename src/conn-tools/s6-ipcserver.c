/* ISC license. */

#include <sys/types.h>
#include <limits.h>
#include <skalibs/uint.h>
#include <skalibs/gidstuff.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <s6/config.h>

#define USAGE "s6-ipcserver [ -q | -Q | -v ] [ -d | -D ] [ -P | -p ] [ -1 ] [ -c maxconn ] [ -C localmaxconn ] [ -b backlog ] [ -G gid,gid,... ] [ -g gid ] [ -u uid ] [ -U ] path prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  unsigned int verbosity = 1 ;
  int flag1 = 0 ;
  int flagU = 0 ;
  int flaglookup = 1 ;
  int flagreuse = 1 ;
  unsigned int uid = 0, gid = 0 ;
  gid_t gids[NGROUPS_MAX] ;
  unsigned int gidn = (unsigned int)-1 ;
  unsigned int maxconn = 0 ;
  unsigned int localmaxconn = 0 ;
  unsigned int backlog = (unsigned int)-1 ;
  PROG = "s6-ipcserver" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "qQvDd1UPpc:C:b:u:g:G:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'q' : verbosity = 0 ; break ;
        case 'Q' : verbosity = 1 ; break ;
        case 'v' : verbosity = 2 ; break ;
        case 'D' : flagreuse = 0 ; break ;
        case 'd' : flagreuse = 1 ; break ;
        case 'P' : flaglookup = 0 ; break ;
        case 'p' : flaglookup = 1 ; break ;
        case 'c' : if (!uint0_scan(l.arg, &maxconn)) dieusage() ; if (!maxconn) maxconn = 1 ; break ;
        case 'C' : if (!uint0_scan(l.arg, &localmaxconn)) dieusage() ; if (!localmaxconn) localmaxconn = 1 ; break ;
        case 'b' : if (!uint0_scan(l.arg, &backlog)) dieusage() ; break ;
        case 'u' : if (!uint0_scan(l.arg, &uid)) dieusage() ; break ;
        case 'g' : if (!uint0_scan(l.arg, &gid)) dieusage() ; break ;
        case 'G' : if (!gid_scanlist(gids, NGROUPS_MAX, l.arg, &gidn) && *l.arg) dieusage() ; break ;
        case '1' : flag1 = 1 ; break ;
        case 'U' : flagU = 1 ; uid = 0 ; gid = 0 ; gidn = (unsigned int)-1 ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (argc < 2) dieusage() ;
  }

  {
    unsigned int m = 0 ;
    unsigned int pos = 0 ;
    char fmt[UINT_FMT * 5 + GID_FMT * NGROUPS_MAX] ;
    char const *newargv[24 + argc] ;
    newargv[m++] = S6_BINPREFIX "s6-ipcserver-socketbinder" ;
    if (!flagreuse) newargv[m++] = "-D" ;
    if (backlog != (unsigned int)-1)
    {
      if (!backlog) backlog = 1 ;
      newargv[m++] = "-b" ;
      newargv[m++] = fmt + pos ;
      pos += uint_fmt(fmt + pos, backlog) ;
      fmt[pos++] = 0 ;
    }
    newargv[m++] = "--" ;
    newargv[m++] = *argv++ ;
    if (flagU || uid || gid || gidn != (unsigned int)-1)
    {
      newargv[m++] = S6_BINPREFIX "s6-applyuidgid" ;
      if (flagU) newargv[m++] = "-Uz" ;
      if (uid)
      {
        newargv[m++] = "-u" ;
        newargv[m++] = fmt + pos ;
        pos += uint_fmt(fmt + pos, uid) ;
        fmt[pos++] = 0 ;
      }
      if (gid)
      {
        newargv[m++] = "-g" ;
        newargv[m++] = fmt + pos ;
        pos += uint_fmt(fmt + pos, gid) ;
        fmt[pos++] = 0 ;
      }
      if (gidn != (unsigned int)-1)
      {
        newargv[m++] = "-G" ;
        newargv[m++] = fmt + pos ;
        pos += gid_fmtlist(fmt + pos, gids, gidn) ;
        fmt[pos++] = 0 ;
      }
      newargv[m++] = "--" ;
    }
    newargv[m++] = S6_BINPREFIX "s6-ipcserverd" ;
    if (!verbosity) newargv[m++] = "-v0" ;
    else if (verbosity == 2) newargv[m++] = "-v2" ;
    if (flag1) newargv[m++] = "-1" ;
    if (!flaglookup) newargv[m++] = "-P" ;
    if (maxconn)
    {
      newargv[m++] = "-c" ;
      newargv[m++] = fmt + pos ;
      pos += uint_fmt(fmt + pos, maxconn) ;
      fmt[pos++] = 0 ;
    }
    if (localmaxconn)
    {
      newargv[m++] = "-C" ;
      newargv[m++] = fmt + pos ;
      pos += uint_fmt(fmt + pos, localmaxconn) ;
      fmt[pos++] = 0 ;
    }
    newargv[m++] = "--" ;
    while (*argv) newargv[m++] = *argv++ ;
    newargv[m++] = 0 ;
    pathexec_run(newargv[0], newargv, envp) ;
    strerr_dieexec(111, newargv[0]) ;
  }
}
