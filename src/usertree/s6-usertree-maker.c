/* ISC license. */

#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <errno.h>

#include <skalibs/config.h>
#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/skamisc.h>

#include <execline/config.h>

#include <s6/config.h>
#include <s6/auto.h>

#define USAGE "s6-usertree-maker [ -d userscandir ] [ -p path ] [ -E envdir [ -e var ... ] ] [ -r service/logger[/pipeline] ] [ -l loguser ] [ -t stamptype ] [ -n nfiles ] [ -s filesize ] [ -S maxsize ] [ -P prefix ] user logdir dir"
#define dieusage() strerr_dieusage(100, USAGE)

#define VARS_MAX 64

static stralloc sa = STRALLOC_ZERO ;

typedef struct svinfo_s svinfo, *svinfo_ref ;
struct svinfo_s
{
  char const *user ;
  char const *sc ;
  char const *logger ;
  char const *path ;
  char const *userenvdir ;
  char const **vars ;
  size_t varlen ;
} ;

static int write_run (buffer *b, void *data)
{
  svinfo *t = data ;
  if (!string_quote(&sa, t->user, strlen(t->user))) return 0 ;
  if (buffer_puts(b, "#!" EXECLINE_SHEBANGPREFIX "execlineb -P\n"
    EXECLINE_EXTBINPREFIX "fdmove -c 2 1\n"
    EXECLINE_EXTBINPREFIX "emptyenv -p\n"
    EXECLINE_EXTBINPREFIX "export USER ") < 0
   || buffer_put(b, sa.s, sa.len) < 0
   || buffer_puts(b, "\n"
    S6_EXTBINPREFIX "s6-envuidgid -i -- ") < 0
   || buffer_put(b, sa.s, sa.len) < 0
   || buffer_puts(b, "\n"
    S6_EXTBINPREFIX "s6-applyuidgid -U --\n"
    EXECLINE_EXTBINPREFIX "backtick -in HOME { "
    EXECLINE_EXTBINPREFIX "homeof ") < 0
   || buffer_put(b, sa.s, sa.len) < 0
   || buffer_put(b, " }\n", 3) < 0) goto err ;
  sa.len = 0 ;
  if (t->userenvdir)
  {
    if (!string_quote(&sa, t->userenvdir, strlen(t->userenvdir))) return 0 ;
    if (buffer_puts(b, S6_EXTBINPREFIX "s6-envdir -i -- ") < 0
     || buffer_put(b, sa.s, sa.len) < 0
     || buffer_put(b, "\n", 1) < 0) goto err ;
    sa.len = 0 ;
    if (t->varlen)
    {
      if (buffer_puts(b, EXECLINE_EXTBINPREFIX "multisubstitute\n{\n") < 0) return 0 ;
      for (size_t i = 0 ; i < t->varlen ; i++)
      {
        if (!string_quote(&sa, t->vars[i], strlen(t->vars[i]))) return 0 ;
        if (buffer_puts(b, "  importas -D \"\" -- ") < 0
         || buffer_put(b, sa.s, sa.len) < 0
         || buffer_put(b, " ", 1) < 0
         || buffer_put(b, sa.s, sa.len) < 0
         || buffer_put(b, "\n", 1) < 0) goto err ;
        sa.len = 0 ;
      }
      if (buffer_put(b, "}\n", 2) < 0) return 0 ;
    }
  }
  if (buffer_puts(b, EXECLINE_EXTBINPREFIX "multisubstitute\n{\n"
   "  importas -i USER USER\n"
   "  importas -i HOME HOME\n"
   "  importas -i UID UID\n"
   "  importas -i GID GID\n"
   "  importas -i GIDLIST GIDLIST\n}\n") < 0) return 0 ;
  if (t->userenvdir && t->varlen)
  {
    for (size_t i = 0 ; i < t->varlen ; i++)
    {
      if (!string_quote(&sa, t->vars[i], strlen(t->vars[i]))) return 0 ;
      if (buffer_puts(b, EXECLINE_EXTBINPREFIX "export ") < 0
       || buffer_put(b, sa.s, sa.len) < 0
       || buffer_put(b, " ${", 3) < 0
       || buffer_put(b, sa.s, sa.len) < 0
       || buffer_put(b, "}\n", 2) < 0) goto err ;
      sa.len = 0 ;
    }
  }
  if (!string_quote(&sa, t->path, strlen(t->path))) return 0 ;
  if (buffer_puts(b, EXECLINE_EXTBINPREFIX "export PATH ") < 0
   || buffer_put(b, sa.s, sa.len) < 0) goto err ;
  sa.len = 0 ;
  if (!string_quote(&sa, t->sc, strlen(t->sc))) return 0 ;
  if (buffer_puts(b, "\n"
    S6_EXTBINPREFIX "s6-svscan -d3 -- ") < 0
   || buffer_put(b, sa.s, sa.len) < 0) goto err ;
  sa.len = 0 ;
  if (!buffer_putflush(b, "\n", 1)) return 0 ;
  return 1 ;

 err:
  sa.len = 0 ;
  return 0 ;
}

int main (int argc, char *const *argv)
{
  char const *vars[VARS_MAX] ;
  svinfo t =
  {
    .sc = "${HOME}/service",
    .logger = 0,
    .path = SKALIBS_DEFAULTPATH,
    .userenvdir = 0,
    .vars = vars,
    .varlen = 0
  } ;
  char *rcinfo[3] = { 0, 0, 0 } ;
  char const *loguser = 0 ;
  unsigned int stamptype = 1 ;
  unsigned int nfiles = 10 ;
  uint64_t filesize = 1000000 ;
  uint64_t maxsize = 0 ;
  char const *prefix = 0 ;
  size_t dirlen ;
  PROG = "s6-usertree-maker" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, (char const *const *)argv, "d:p:E:e:r:l:t:n:s:S:P:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'd' : t.sc = l.arg ; break ;
        case 'p' : t.path = l.arg ; break ;
        case 'E' : t.userenvdir = l.arg ; break ;
        case 'e' :
          if (t.varlen >= VARS_MAX) strerr_dief1x(100, "too many -v variables") ;
          if (strchr(l.arg, '=')) strerr_dief2x(100, "invalid variable name: ", l.arg) ;
          t.vars[t.varlen++] = l.arg ;
          break ;
        case 'r' : rcinfo[0] = (char *)l.arg ; break ;
        case 'l' : loguser = l.arg ; break ;
        case 't' : if (!uint0_scan(l.arg, &stamptype)) dieusage() ; break ;
        case 'n' : if (!uint0_scan(l.arg, &nfiles)) dieusage() ; break ;
        case 's' : if (!uint640_scan(l.arg, &filesize)) dieusage() ; break ;
        case 'S' : if (!uint640_scan(l.arg, &maxsize)) dieusage() ; break ;
        case 'P' : prefix = l.arg ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (argc < 3) dieusage() ;
  if (argv[1][0] != '/') strerr_dief1x(100, "logdir must be absolute") ;
  if (t.sc[0] != '/' && !str_start(t.sc, "${HOME}/"))
    strerr_dief1x(100, "userscandir must be absolute or start with ${HOME}/") ;
  if (stamptype > 3) strerr_dief1x(100, "stamptype must be 0, 1, 2 or 3") ;
  if (rcinfo[0])
  {
    if (strchr(rcinfo[0], '\n'))
      strerr_dief2x(100, "newlines", " are forbidden in s6-rc names") ;
    if (rcinfo[0][0] == '/')
      strerr_dief2x(100, "service", " name cannot be empty") ;
    rcinfo[1] = strchr(rcinfo[0], '/') ;
    if (!rcinfo[1]) strerr_dief1x(100, "argument to -r must be: service/logger or service/logger/pipeline") ;
    *rcinfo[1]++ = 0 ;
    if (!rcinfo[1][0]) strerr_dief1x(100, "argument to -r must be: service/logger or service/logger/pipeline") ;
    if (rcinfo[1][0] == '/')
      strerr_dief2x(100, "logger", " name cannot be empty") ;
    rcinfo[2] = strchr(rcinfo[1], '/') ;
    if (rcinfo[2])
    {
      *rcinfo[2]++ = 0 ;
      if (!rcinfo[2][0]) strerr_dief2x(100, "pipeline", " name cannot be empty") ;
      if (strchr(rcinfo[2], '/')) strerr_dief2x(100, "slashes", " are forbidden in s6-rc names") ;
    }
  }
  t.user = argv[0] ;
  dirlen = strlen(argv[2]) ;

  if (rcinfo[0])
  {
    size_t svclen = strlen(rcinfo[0]) ;
    size_t loglen = strlen(rcinfo[1]) ;
    char dir[dirlen + 2 + (svclen > loglen ? svclen : loglen)] ;
    memcpy(dir, argv[2], dirlen) ;
    dir[dirlen] = '/' ;
    if (mkdir(argv[2], 0755) < 0 && errno != EEXIST)
      strerr_diefu2sys(111, "mkdir ", argv[2]) ;
    t.logger = rcinfo[1] ;
    memcpy(dir + dirlen + 1, rcinfo[0], svclen + 1) ;
    s6_auto_write_service(dir, 3, &write_run, &t, rcinfo[1]) ;
    memcpy(dir + dirlen + 1, rcinfo[1], loglen + 1) ;
    s6_auto_write_logger_tmp(dir, loguser, argv[1], stamptype, nfiles, filesize, maxsize, prefix, rcinfo[0], rcinfo[2], &sa) ;
  }
  else
  {
    char dir[dirlen + 5] ;
    memcpy(dir, argv[2], dirlen) ;
    memcpy(dir + dirlen, "/log", 5) ;
    s6_auto_write_service(argv[2], 3, &write_run, &t, 0) ;
    s6_auto_write_logger_tmp(dir, loguser, argv[1], stamptype, nfiles, filesize, maxsize, prefix, 0, 0, &sa) ;
  }
  return 0 ;
}
