/* ISC license. */

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr2.h>

#include <s6/s6-supervise.h>

#define USAGE "s6-svperms [ -v ] [ -u | -g group | -G group | -o | -O group ] [ -e | -E group ] servicedir..."
#define dieusage() strerr_dieusage(100, USAGE)

static gid_t scangid (char const *s)
{
  if (s[0] == ':')
  {
    gid_t g ;
    if (!gid0_scan(s+1, &g)) dieusage() ;
    return g ;
  }
  else
  {
    struct group *gr ;
    errno = 0 ;
    gr = getgrnam(s) ;
    if (!gr)
    {
      if (errno) strerr_diefu1sys(111, "getgrnam") ;
      else strerr_diefu3x(100, "find entry for ", s, " in group database") ;
    }
    return gr->gr_gid ;
  }
}

static char *gidname (gid_t gid)
{
  struct group *gr ;
  errno = 0 ;
  gr = getgrgid(gid) ;
  if (!gr)
  {
    static char fmt[GID_FMT] ;
    fmt[gid_fmt(fmt, gid)] = 0 ;
    if (errno) strerr_warnwu2sys("getgrgid ", fmt) ;
    return fmt ;
  }
  return gr->gr_name ;
}

static void out (char const *s)
{
  if (buffer_puts(buffer_1, s) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

static inline int printsupervise (char const *dir)
{
  struct stat st ;
  size_t len = strlen(dir) ;
  char fn[len + sizeof(S6_SUPERVISE_CTLDIR) + 9] ;
  memcpy(fn, dir, len) ;
  memcpy(fn + len, "/" S6_SUPERVISE_CTLDIR, sizeof(S6_SUPERVISE_CTLDIR) + 1) ;
  if (stat(fn, &st) < 0)
  {
    strerr_warnwu2sys("stat ", fn) ;
    return 1 ;
  }
  if (!S_ISDIR(st.st_mode))
  {
    strerr_warnw2x(fn, " is not a directory") ;
    return 1 ;
  }
  if (st.st_mode & 05066 || (st.st_mode & 0700) != 0700 || ((st.st_mode & 0001) && !(st.st_mode & 0010)))
  {
    char fmt[UINT_OFMT] ;
    fmt[uint_ofmt(fmt, st.st_mode & 07777)] = 0 ;
    strerr_warnw3x(fn, " has incorrect permissions: ", fmt) ;
    return 1 ;
  }
  out(dir) ;
  out(" status: ") ;
  if (st.st_mode & 0011)
  {
    if (st.st_mode & 0001) buffer_puts(buffer_1, "public") ;
    else
    {
      out("group ") ;
      out(gidname(st.st_gid)) ;
    }
  }
  else out("owner") ;
  out("\n") ;
  memcpy(fn + len + sizeof(S6_SUPERVISE_CTLDIR), "/control", 9) ;
  if (stat(fn, &st) < 0)
  {
    strerr_warnwu2sys("stat ", fn) ;
    return 1 ;
  }
  if (!S_ISFIFO(st.st_mode))
  {
    strerr_warnw2x(fn, " is not a named pipe") ;
    return 1 ;
  }
  if (st.st_mode & 0157)
  {
    char fmt[UINT_OFMT] ;
    fmt[uint_ofmt(fmt, st.st_mode & 07777)] = 0 ;
    strerr_warnw3x(fn, " has incorrect permissions: ", fmt) ;
    return 1 ;
  }
  out(dir) ;
  out(" control: ") ;
  if (st.st_mode & 0020)
  {
    out("group ") ;
    out(gidname(st.st_gid)) ;
  }
  else out("owner") ;
  out("\n") ;
  return 0 ;
}

static inline int printevent (char const *dir)
{
  struct stat st ;
  size_t len = strlen(dir) ;
  char fn[len + sizeof(S6_SUPERVISE_EVENTDIR) + 1] ;
  memcpy(fn, dir, len) ;
  memcpy(fn + len, "/" S6_SUPERVISE_EVENTDIR, sizeof(S6_SUPERVISE_EVENTDIR) + 1) ;
  if (stat(fn, &st) < 0)
  {
    strerr_warnwu2sys("stat ", fn) ;
    return 1 ;
  }
  if (!S_ISDIR(st.st_mode))
  {
    strerr_warnw2x(fn, " is not a directory") ;
    return 1 ;
  }
  if ((st.st_mode & 07777) != 01733 && (st.st_mode & 07777) != 03730)
  {
    char fmt[UINT_OFMT] ;
    fmt[uint_ofmt(fmt, st.st_mode & 07777)] = 0 ;
    strerr_warnw3x(fn, " has incorrect permissions: ", fmt) ;
    return 1 ;
  }
  out(dir) ;
  out(" events: ") ;
  if ((st.st_mode & 07777) == 03730)
  {
    out("group ") ;
    out(gidname(st.st_gid)) ;
  }
  else out("public") ;
  out("\n") ;
  return 0 ;
}

static gid_t primarygid (char const *fn)
{
  struct passwd *pw ;
  struct stat st ;
  if (stat(fn, &st) < 0) strerr_diefu2sys(111, "stat ", fn) ;
  errno = 0 ;
  pw = getpwuid(st.st_uid) ;
  if (!pw)
  {
    strerr_warnwu3sys("determine primary gid for the owner of ", fn, " (using root instead)") ;
    return 0 ;
  }
  else return pw->pw_gid ;
}

static inline void modsupervise (char const *dir, unsigned int what, gid_t gid)
{
  size_t len = strlen(dir) ;
  gid_t cgid  = 0 ;
  mode_t mode = 0700 ;
  char fn[len + sizeof(S6_SUPERVISE_CTLDIR) + 9] ;
  memcpy(fn, dir, len) ;
  memcpy(fn + len, "/" S6_SUPERVISE_CTLDIR, sizeof(S6_SUPERVISE_CTLDIR) + 1) ;
  switch (what & 3)
  {
    case 0 : cgid = primarygid(fn) ; mode = 0700 ; break ;
    case 1 : cgid = gid ; mode = 0710 ; break ;
    case 2 : cgid = primarygid(fn) ; mode = 0711 ; break ;
  }
  if (chown(fn, -1, cgid) < 0)
    strerr_diefu2sys(111, "chown ", fn) ;
  if (chmod(fn, mode) < 0)
    strerr_diefu2sys(111, "chmod ", fn) ;
  memcpy(fn + len + sizeof(S6_SUPERVISE_CTLDIR), "/control", 9) ;
  if (what & 4) mode = 0620 ;
  else
  {
    gid = primarygid(fn) ;
    mode = 0600 ;
  }
  if (chown(fn, -1, gid) < 0)
    strerr_diefu2sys(111, "chown ", fn) ;
  if (chmod(fn, mode) < 0)
    strerr_diefu2sys(111, "chmod ", fn) ;
}

static inline void modevent (char const *dir, gid_t gid)
{
  size_t len = strlen(dir) ;
  mode_t mode ;
  char fn[len + sizeof(S6_SUPERVISE_EVENTDIR) + 1] ;
  memcpy(fn, dir, len) ;
  memcpy(fn + len, "/" S6_SUPERVISE_EVENTDIR, sizeof(S6_SUPERVISE_EVENTDIR) + 1) ;
  if (gid == (gid_t)-1)
  {
    gid = primarygid(fn) ;
    mode = 01733 ;
  }
  else mode = 03730 ;
  if (chown(fn, -1, gid) < 0)
    strerr_diefu2sys(111, "chown ", fn) ;
  if (chmod(fn, mode) < 0)
    strerr_diefu2sys(111, "chmod ", fn) ;
}

int main (int argc, char const *const *argv)
{
  int e = 0 ;
  gid_t gid = -1 ;
  gid_t eventgid = -1 ;
  int rw = 0 ;
  unsigned int what = 0 ;
  PROG = "s6-svperms" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "vug:G:oO:eE:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'v' : rw |= 1 ; break ;
        case 'u' : rw |= 2 ; what = 0 ; break ;
        case 'g' : rw |= 2 ; what = 1 ; gid = scangid(l.arg) ; break ;
        case 'G' : rw |= 2 ; what = 5 ; gid = scangid(l.arg) ; break ;
        case 'o' : rw |= 2 ; what = 2 ; break ;
        case 'O' : rw |= 2 ; what = 6 ; gid = scangid(l.arg) ; break ;
        case 'e' : rw |= 4 ; eventgid = -1 ; break ;
        case 'E' : rw |= 4 ; eventgid = scangid(l.arg) ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) dieusage() ;

  if (!rw) rw = 1 ;
  for (; *argv ; argv++)
  {
    if (rw & 2) modsupervise(*argv, what, gid) ;
    if (rw & 4) modevent(*argv, eventgid) ;
    if (rw & 1) { e |= printsupervise(*argv) ; e |= printevent(*argv) ; }
  }
  if (rw & 1 && !buffer_flush(buffer_1))
    strerr_diefu1sys(111, "write to stdout") ;
  return e ;
}
