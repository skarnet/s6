 /* ISC license. */

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>

#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr.h>
#include <skalibs/exec.h>

#include <s6/config.h>

#ifdef S6_USE_EXECLINE
# include <execline/config.h>
# define USAGE "s6-alias-chpst [ -v ] [ -P ] [ -0 ] [ -1 ] [ -2 ] [ -u user ] [ -U user ] [ -b argv0 ] [ -e dir ] [ -n niceness ] [ -l lock | -L lock ] [ -m bytes ] [ -d bytes ] [ -o n ] [ -p n ] [ -f bytes ] [ -c bytes ] prog..."
#else
# define USAGE "s6-alias-chpst [ -v ] [ -P ] [ -u user ] [ -U user ] [ -e dir ] [ -n niceness ] [ -l lock | -L lock ] [ -m bytes ] [ -d bytes ] [ -o n ] [ -p n ] [ -f bytes ] [ -c bytes ] prog..."
#endif

#define dienomem() strerr_diefu1sys(111, "stralloc_catb")
#define dieusage() strerr_dieusage(100, USAGE)

static unsigned int verbosity = 0 ;

static void printit (char const *const *argv)
{
  buffer_puts(buffer_2, PROG) ;
  buffer_puts(buffer_2, ": info: executing the following command line:") ;
  for (; *argv ; argv++)
  {
    buffer_puts(buffer_2, " ") ;
    buffer_puts(buffer_2, *argv) ;
  }
  buffer_putsflush(buffer_2, "\n") ;
}

static inline size_t parseuggnum (char const *s, uint32_t *flags, uid_t *uid, gid_t *gid, gid_t *tab)
{
  size_t n = 0 ;
  size_t pos = uid_scan(s, uid) ;
  if (!pos) dieusage() ;
  if (!s[pos]) return 0 ;
  if (s[pos] != ':') dieusage() ;
  s += pos+1 ;
  pos = gid_scan(s, gid) ;
  if (!pos) dieusage() ;
  *flags |= 32768 ;
  if (!s[pos]) return 0 ;
  if (s[pos] != ':') dieusage() ;
  s += pos+1 ;
  if (!gid_scanlist(tab, NGROUPS_MAX, s, &n)) dieusage() ;
  return n ;
}

static struct passwd *do_getpwnam (char const *s)
{
  struct passwd *pw = getpwnam(s) ;
  if (!pw)
  {
    if (errno) strerr_diefu1sys(111, "read user database") ;
    else strerr_dief2x(100, "user not found in user database: ", s) ;
  }
  return pw ;
}

static struct group *do_getgrnam (char const *s)
{
  struct group *gr = getgrnam(s) ;
  if (!gr)
  {
    if (errno) strerr_diefu1sys(111, "read group database") ;
    else strerr_dief2x(100, "group not found in group database: ", s) ;
  }
  return gr ;
}

static inline size_t parseuggsym (char const *s, uint32_t *flags, uid_t *uid, gid_t *gid, gid_t *tab)
{
  size_t n = 0 ;
  struct passwd *pw ;
  struct group *gr ;
  size_t pos = str_chr(s, ':') ;
  *flags |= 32768 ;
  errno = 0 ;
  if (!s[pos]) pw = do_getpwnam(s) ;
  else
  {
    char tmp[pos+1] ;
    memcpy(tmp, s, pos) ;
    tmp[pos] = 0 ;
    pw = do_getpwnam(tmp) ;
  }
  *uid = pw->pw_uid ;
  if (!s[pos])
  {
    *gid = pw->pw_gid ;
    return 0 ;
  }
  s += pos+1 ;
  pos = str_chr(s, ':') ;
  errno = 0 ;
  if (!s[pos]) gr = do_getgrnam(s) ;
  else
  {
    char tmp[pos+1] ;
    memcpy(tmp, s, pos) ;
    tmp[pos] = 0 ;
    gr = do_getgrnam(tmp) ;
  }
  *gid = gr->gr_gid ;
  if (!s[pos]) return 0 ;
  s += pos+1 ;
  while (*s)
  {
    if (n >= NGROUPS_MAX)
      strerr_dief1x(100, "too many supplementary groups listed for the -u option") ;
    pos = str_chr(s, ':') ;
    errno = 0 ;
    if (!s[pos]) gr = do_getgrnam(s) ;
    else
    {
      char tmp[pos+1] ;
      memcpy(tmp, s, pos) ;
      tmp[pos] = 0 ;
      gr = do_getgrnam(tmp) ;
    }
    tab[n++] = gr->gr_gid ;
    s += s[pos] ? pos+1 : pos ;
  }
  return n ;
}

int main (int argc, char const *const *argv)
{
  static char const *valopt[6] = { "-m", "-d", "-o", "-p", "-f", "-c" } ;
  unsigned int newargc = 0 ;
  char const *newroot = 0 ;
  char const *edir = 0 ;
  char const *lockfile = 0 ;
  char const *envug = 0 ;
#ifdef S6_USE_EXECLINE
  char const *argv0 = 0 ;
#endif
  uint32_t flags = 0 ;
  int niceval = 0 ;
  char valfmt[6][UINT64_FMT] ;
  char nicefmt[INT_FMT] ;
  char uidfmt[UID_FMT] ;
  char gidfmt[GID_FMT] ;
  char gidlistfmt[GID_FMT * NGROUPS_MAX] ;
  PROG = "s6-alias-chpst" ;

  {
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
#ifdef S6_USE_EXECLINE
      int opt = subgetopt_r(argc, argv, "vP012u:U:b:e:/:n:l:L:m:d:o:p:f:c:", &l) ;
#else
      int opt = subgetopt_r(argc, argv, "vPu:U:e:/:n:l:L:m:d:o:p:f:c:", &l) ;
#endif
      if (opt == -1) break ;
      switch (opt)
      {
        case 'v' : verbosity++ ; break ;
        case 'P' : flags |= 8 ; newargc += 2 ; break ;
#ifdef S6_USE_EXECLINE
        case '0' : flags |= 1 ; newargc += 2 ; break ;
        case '1' : flags |= 2 ; newargc += 2 ; break ;
        case '2' : flags |= 4 ; newargc += 2 ; break ;
        case 'b' : argv0 = l.arg ; newargc += 4 ; break ;
#endif
        case 'u' :
        {
          size_t n ;
          uid_t uid ;
          gid_t gid ;
          gid_t tab[NGROUPS_MAX] ;
          newargc += 6 ;
          flags |= 16384 ; flags &= ~32768 ;
          n = l.arg[0] == ':' ?
            parseuggnum(l.arg + 1, &flags, &uid, &gid, tab) :
            parseuggsym(l.arg, &flags, &uid, &gid, tab) ;
          uidfmt[uid_fmt(uidfmt, uid)] = 0 ;
          if (flags & 32768)
          {
            newargc += 2 ;
            gidfmt[gid_fmt(gidfmt, gid)] = 0 ;
          }
          gidlistfmt[gid_fmtlist(gidlistfmt, tab, n)] = 0 ;
          break ;
        }
        case 'U' :
          envug = l.arg ;
          newargc += 3 ;
          if (envug[0] == ':') { flags |= 4096 ; envug++ ; newargc++ ; } else flags &= ~4096 ;
          if (strchr(envug, ':')) { flags |= 8192 ; newargc++ ; } else flags &= ~8192 ;
          break ;
        case 'e' : edir = l.arg ; newargc += 3 ; break ;
        case '/' : newroot = l.arg ; newargc += 2 ; break ;
        case 'n' :
          if (!int0_scan(l.arg, &niceval)) dieusage() ;
          newargc += 3 ;
          break ;
        case 'l' : lockfile = l.arg ; flags &= ~16 ; newargc += 4 ; break ;
        case 'L' : lockfile = l.arg ; flags |= 16 ; newargc += 4 ; break ;
        case 'm' :
        case 'd' :
        case 'o' :
        case 'p' :
        case 'f' :
        case 'c' :
        {
          uint64_t val ;
          size_t pos = byte_chr("mdopfc", 6, opt) ;
          if (!uint640_scan(l.arg, &val)) dieusage() ;
          valfmt[pos][uint64_fmt(valfmt[pos], val)] = 0 ;
          flags |= 32 | (1 << (6 + pos)) ;
          newargc += 2 ;
          break ;
        }
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }

  if (newroot)
  {
    newroot = realpath(newroot, 0) ;
    if (!newroot) dienomem() ;
  }
  if (flags & 32) newargc += 2 ;
  newargc += argc ;

  {
    unsigned int m = 0 ;
    char const *newargv[newargc + 1] ;

    if (niceval)
    {
      nicefmt[int_fmt(nicefmt, niceval)] = 0 ;
      newargv[m++] = "nice" ;
      newargv[m++] = "-n" ;
      newargv[m++] = nicefmt ;
      newargv[m++] = "--" ;
    }

    if (flags & 8)
    {
      newargv[m++] = S6_BINPREFIX "s6-setsid" ;
      newargv[m++] = "--" ;
    }

    if (edir)
    {
      newargv[m++] = S6_BINPREFIX "s6-envdir" ;
      newargv[m++] = "--" ;
      newargv[m++] = edir ;
    }

    if (lockfile)
    {
      newargv[m++] = S6_BINPREFIX "s6-setlock" ;
      newargv[m++] = flags & 16 ? "-n" : "-N" ;
      newargv[m++] = "--" ;
      newargv[m++] = lockfile ;
    }

    if (flags & 16384)
    {
      newargv[m++] = S6_BINPREFIX "s6-applyuidgid" ;
      newargv[m++] = "-u" ;
      newargv[m++] = uidfmt ;
      if (flags & 32768)
      {
        newargv[m++] = "-g" ;
        newargv[m++] = gidfmt ;
      }
      newargv[m++] = "-G" ;
      newargv[m++] = gidlistfmt ;
      newargv[m++] = "--" ;
    }

    if (envug)
    {
      newargv[m++] = S6_BINPREFIX "s6-envuidgid" ;
      if (flags & 4096) newargv[m++] = "-n" ;
      if (flags & 8192) newargv[m++] = "-B" ;
      newargv[m++] = "--" ;
      newargv[m++] = envug ;
    }

    if (flags & 32)
    {
      newargv[m++] = S6_BINPREFIX "s6-softlimit" ;
      for (unsigned int i = 0 ; i < 6 ; i++) if (flags & (1 << (i + 6)))
      {
        newargv[m++] = valopt[i] ;
        newargv[m++] = valfmt[i] ;
      }
      newargv[m++] = "--" ;
    }

#ifdef S6_USE_EXECLINE
    if (flags & 1)
    {
      newargv[m++] = EXECLINE_EXTBINPREFIX "fdclose" ;
      newargv[m++] = "0" ;
    }

    if (flags & 2)
    {
      newargv[m++] = EXECLINE_EXTBINPREFIX "fdclose" ;
      newargv[m++] = "1" ;
    }

    if (flags & 4)
    {
      newargv[m++] = EXECLINE_EXTBINPREFIX "fdclose" ;
      newargv[m++] = "2" ;
    }

    if (argv0 && newroot)
    {
      argv0 = 0 ;
      strerr_warnw1x("the -b option is ineffective when the -/ option is also given") ;
    }

    if (argv0)
    {
      newargv[m++] = EXECLINE_EXTBINPREFIX "exec" ;
      newargv[m++] = "-a" ;
      newargv[m++] = argv0 ;
      newargv[m++] = "--" ;
    }
#endif

    if (newroot)
    {
      newargv[m++] = "chroot" ;
      newargv[m++] = newroot ;
    }

    for (int i = 0 ; i < argc+1 ; i++) newargv[m++] = argv[i] ;
    if (verbosity) printit(newargv) ;
    xexec0(newargv) ;
  }
}
