/* ISC license. */

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/skamisc.h>

#include <execline/config.h>

#include <s6/config.h>
#include <s6/auto.h>

#define USAGE "s6-instance-maker [ -c max ] [ -u user ] [ -l loguser ] [ -L logdir ] [ -t stamptype ] [ -n nfiles ] [ -s filesize ] [ -S maxsize ] [ -P prefix ] [ -r service[/logger[/pipeline]] ] template dir"
#define dieusage() strerr_dieusage(100, USAGE)

typedef struct svinfo_s svinfo, *svinfo_ref ;
struct svinfo_s
{
  char const *user ;
  unsigned int maxinstances ;
} ;

static stralloc sa = STRALLOC_ZERO ;

static int write_run (buffer *b, void *data)
{
  svinfo *t = data ;
  size_t l ;
  char fmt[UINT_FMT] ;
  l = uint_fmt(fmt, t->maxinstances) ;
  if (buffer_puts(b, "#!" EXECLINE_SHEBANGPREFIX "execlineb -P\n\n"
    EXECLINE_EXTBINPREFIX "fdmove -c 2 1\n") < 0) return 0 ;
  if (t->user[0])
  {
    if (!string_quote(&sa, t->user, strlen(t->user))) return 0 ;
    if (buffer_puts(b, S6_EXTBINPREFIX "s6-setuidgid ") < 0
     || buffer_put(b, sa.s, sa.len) < 0
     || buffer_put(b, "\n", 1) < 0) goto err ;
    sa.len = 0 ;
  }
  if (buffer_puts(b, S6_EXTBINPREFIX "s6-svscan -d3 -c") < 0
   || buffer_put(b, fmt, l) < 0
   || buffer_putsflush(b, " -- instance\n") < 0) return 0 ;
  return 1 ;

err:
  sa.len = 0 ;
  return 0 ;
}

static void write_service (char const *dir, char const *template, char const *user, unsigned int maxinstances, char const *logger)
{
  svinfo data = { .user = user, .maxinstances = maxinstances } ;
  size_t dirlen = strlen(dir) ;
  char fn[dirlen + 11] ;
  memcpy(fn, dir, dirlen) ;
  s6_auto_write_service(dir, 3, &write_run, &data, logger) ;
  if (!logger)
  {
    mode_t m = umask(0) ;
    memcpy(fn + dirlen, "/instance", 10) ;
    if (mkdir(fn, 0755) == -1) strerr_diefu2sys(111, "mkdir ", fn) ;
    memcpy(fn + dirlen + 9, "s", 2) ;
    if (mkdir(fn, 0755) == -1) strerr_diefu2sys(111, "mkdir ", fn) ;
    umask(m) ;
  }
  memcpy(fn + dirlen, "/template", 10) ;
  if (!hiercopy_tmp(template, fn, &sa))
    strerr_diefu4sys(111, "copy file hierarchy from ", template, " to ", fn) ;
}

int main (int argc, char const *const *argv)
{
  char const *user = "" ;
  char const *loguser = 0 ;
  char const *logdir = 0 ;
  unsigned int maxinstances = 500 ;
  unsigned int stamptype = 1 ;
  unsigned int nfiles = 10 ;
  uint64_t filesize = 1000000 ;
  uint64_t maxsize = 0 ;
  char const *prefix = 0 ;
  char *rcinfo[3] = { 0, 0, 0 } ;
  size_t dirlen ;
  PROG = "s6-instance-maker" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "u:l:L:c:t:n:s:S:P:r:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'u' : user = l.arg ; break ;
        case 'l' : loguser = l.arg ; break ;
        case 'L' : logdir = l.arg ; break ;
        case 'c' : if (!uint0_scan(l.arg, &maxinstances)) dieusage() ; break ;
        case 't' : if (!uint0_scan(l.arg, &stamptype)) dieusage() ; break ;
        case 'n' : if (!uint0_scan(l.arg, &nfiles)) dieusage() ; break ;
        case 's' : if (!uint640_scan(l.arg, &filesize)) dieusage() ; break ;
        case 'S' : if (!uint640_scan(l.arg, &maxsize)) dieusage() ; break ;
        case 'P' : prefix = l.arg ; break ;
	case 'r' : rcinfo[0] = (char *)l.arg ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }

  if (argc < 2) dieusage() ;
  if (logdir && logdir[0] != '/')
    strerr_dief1x(100, "logdir must be absolute") ;
  if (stamptype > 3) strerr_dief1x(100, "stamptype must be 0, 1, 2 or 3") ;
  if (strchr(user, ' ') || strchr(user, '\t') || strchr(user, '\n'))
    strerr_dief1x(100, "invalid user") ;
  if (maxinstances < 1) maxinstances = 1 ;
  if (maxinstances > 90000) maxinstances = 90000 ;
  if (rcinfo[0])
  {
    if (strchr(rcinfo[0], '\n'))
      strerr_dief2x(100, "newlines", " are forbidden in s6-rc names") ;
    if (rcinfo[0][0] == '/')
      strerr_dief2x(100, "service", " name cannot be empty") ;
    rcinfo[1] = strchr(rcinfo[0], '/') ;
    if (rcinfo[1])
    {
      *rcinfo[1]++ = 0 ;
      if (!rcinfo[1][0]) strerr_dief1x(100, "argument to -r must be: service or service/logger or service/logger/pipeline") ;
      if (rcinfo[1][0] == '/')
        strerr_dief2x(100, "logger", " name cannot be empty") ;
      if (!logdir) strerr_dief1x(100, "logger specifiec (-r) but logdir not specified (-L)") ;
      rcinfo[2] = strchr(rcinfo[1], '/') ;
      if (rcinfo[2])
      {
        *rcinfo[2]++ = 0 ;
        if (!rcinfo[2][0]) strerr_dief2x(100, "pipeline", " name cannot be empty") ;
        if (strchr(rcinfo[2], '/')) strerr_dief2x(100, "slashes", " are forbidden in s6-rc names") ;
      }
    }
    else if (logdir)
      strerr_dief1x(100, "logdir specified (-L) but logger name not specified (-r)") ;
  }

  dirlen = strlen(argv[1]) ;
  if (rcinfo[0])
  {
    mode_t m = umask(0) ;
    size_t svclen = strlen(rcinfo[0]) ;
    size_t loglen = rcinfo[1] ? strlen(rcinfo[1]) : 0 ;
    char dir[dirlen + 2 + (svclen > loglen ? svclen : loglen)] ;
    memcpy(dir, argv[1], dirlen) ;
    dir[dirlen] = '/' ;
    if (mkdir(argv[1], 0755) < 0 && errno != EEXIST)
      strerr_diefu2sys(111, "mkdir ", argv[1]) ;
    umask(m) ;
    memcpy(dir + dirlen + 1, rcinfo[0], svclen + 1) ;
    write_service(dir, argv[0], user, maxinstances, rcinfo[1] ? rcinfo[1] : "") ;
    if (rcinfo[1])
    {
      memcpy(dir + dirlen + 1, rcinfo[1], loglen + 1) ;
      s6_auto_write_logger_tmp(dir, loguser, logdir, stamptype, nfiles, filesize, maxsize, prefix, rcinfo[0], rcinfo[2], &sa) ;
    }
  }
  else
  {
    write_service(argv[1], argv[0], user, maxinstances, 0) ;
    if (logdir)
    {
      char dir[dirlen + 5] ;
      memcpy(dir, argv[1], dirlen) ;
      memcpy(dir + dirlen, "/log", 5) ;
      s6_auto_write_logger_tmp(dir, loguser, logdir, stamptype, nfiles, filesize, maxsize, prefix, 0, 0, &sa) ;
    }
  }
  return 0 ;
}
