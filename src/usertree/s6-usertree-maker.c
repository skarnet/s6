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
#include <skalibs/strerr2.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/skamisc.h>

#include <execline/config.h>

#include <s6/config.h>

#define USAGE "s6-usertree-maker [ -d userscandir ] [ -p path ] [ -E envdir [ -e var ... ] ] [ -r service/logger[/pipeline] ] [ -l loguser ] [ -t stamptype ] [ -n nfiles ] [ -s filesize ] [ -S maxsize ] user logdir dir"
#define dieusage() strerr_dieusage(100, USAGE)

#define VARS_MAX 64

static mode_t mask ;
static stralloc sa = STRALLOC_ZERO ;

static inline void write_run (char const *runfile, char const *user, char const *sc, char const *path, char const *userenvdir, char const *const *vars, size_t varlen)
{
  buffer b ;
  char buf[2048] ;
  int fd = open_trunc(runfile) ;
  if (fd < 0) strerr_diefu3sys(111, "open ", runfile, " for writing") ;
  buffer_init(&b, &buffer_write, fd, buf, 2048) ;
  if (!string_quote(&sa, user, strlen(user))) goto errq ;
  if (buffer_puts(&b, "#!" EXECLINE_SHEBANGPREFIX "execlineb -P\n"
    EXECLINE_EXTBINPREFIX "fdmove -c 2 1\n"
    EXECLINE_EXTBINPREFIX "emptyenv -p\n"
    EXECLINE_EXTBINPREFIX "export USER ") < 0
   || buffer_put(&b, sa.s, sa.len) < 0
   || buffer_puts(&b, "\n"
    S6_EXTBINPREFIX "s6-envuidgid -i -- ") < 0
   || buffer_put(&b, sa.s, sa.len) < 0
   || buffer_puts(&b, "\n"
    S6_EXTBINPREFIX "s6-applyuidgid -U --\n"
    EXECLINE_EXTBINPREFIX "backtick -in HOME { "
    EXECLINE_EXTBINPREFIX "homeof ") < 0
   || buffer_put(&b, sa.s, sa.len) < 0
   || buffer_put(&b, " }\n", 3) < 0) goto err ;
  sa.len = 0 ;
  if (userenvdir)
  {
    if (!string_quote(&sa, userenvdir, strlen(userenvdir))) goto errq ;
    if (buffer_puts(&b, S6_EXTBINPREFIX "s6-envdir -i -- ") < 0
     || buffer_put(&b, sa.s, sa.len) < 0
     || buffer_put(&b, "\n", 1) < 0) goto err ;
    sa.len = 0 ;
    if (varlen)
    {
      if (buffer_puts(&b, EXECLINE_EXTBINPREFIX "multisubstitute\n{\n") < 0) goto err ;
      for (size_t i = 0 ; i < varlen ; i++)
      {
        if (!string_quote(&sa, vars[i], strlen(vars[i]))) goto errq ;
        if (buffer_puts(&b, "  importas -D \"\" -- ") < 0
         || buffer_put(&b, sa.s, sa.len) < 0
         || buffer_put(&b, " ", 1) < 0
         || buffer_put(&b, sa.s, sa.len) < 0
         || buffer_put(&b, "\n", 1) < 0) goto err ;
        sa.len = 0 ;
      }
      if (buffer_put(&b, "}\n", 2) < 0) goto err ;
    }
  }
  if (buffer_puts(&b, EXECLINE_EXTBINPREFIX "multisubstitute\n{\n"
   "  importas -i USER USER\n"
   "  importas -i HOME HOME\n"
   "  importas -i UID UID\n"
   "  importas -i GID GID\n"
   "  importas -i GIDLIST GIDLIST\n}\n") < 0) goto err ;
  if (userenvdir && varlen)
  {
    for (size_t i = 0 ; i < varlen ; i++)
    {
      if (!string_quote(&sa, vars[i], strlen(vars[i]))) goto errq ;
      if (buffer_puts(&b, EXECLINE_EXTBINPREFIX "export ") < 0
       || buffer_put(&b, sa.s, sa.len) < 0
       || buffer_put(&b, " ${", 3) < 0
       || buffer_put(&b, sa.s, sa.len) < 0
       || buffer_put(&b, "}\n", 2) < 0) goto err ;
      sa.len = 0 ;
    }
  }
  if (!string_quote(&sa, path, strlen(path))) goto errq ;
  if (buffer_puts(&b, EXECLINE_EXTBINPREFIX "export PATH ") < 0
   || buffer_put(&b, sa.s, sa.len) < 0) goto err ;
  sa.len = 0 ;
  if (!string_quote(&sa, sc, strlen(sc))) goto errq ;
  if (buffer_puts(&b, "\n"
    S6_EXTBINPREFIX "s6-svscan -d3 -- ") < 0
   || buffer_put(&b, sa.s, sa.len) < 0) goto err ;
  sa.len = 0 ;
  if (!buffer_putflush(&b, "\n", 1)) goto err ;
  fd_close(fd) ;
  return ;
 err:
  strerr_diefu2sys(111, "write to ", runfile) ;
 errq:
  strerr_diefu1sys(111, "quote string") ;
}

static inline void write_logrun (char const *runfile, char const *loguser, char const *logdir, unsigned int stamptype, unsigned int nfiles, uint64_t filesize, uint64_t maxsize)
{
  buffer b ;
  char buf[1024] ;
  char fmt[UINT64_FMT] ;
  int fd = open_trunc(runfile) ;
  if (fd < 0) strerr_diefu3sys(111, "open ", runfile, " for writing") ;
  buffer_init(&b, &buffer_write, fd, buf, 1024) ;
  if (buffer_puts(&b, "#!" EXECLINE_SHEBANGPREFIX "execlineb -P\n") < 0) goto err ;
  if (loguser)
  {
    if (buffer_puts(&b, S6_EXTBINPREFIX "s6-setuidgid ") < 0) goto err ;
    if (!string_quote(&sa, loguser, strlen(loguser))) strerr_diefu1sys(111, "quote string") ;
    if (buffer_put(&b, sa.s, sa.len) < 0 || buffer_put(&b, "\n", 1) < 0) goto err ;
    sa.len = 0 ;
  }
  if (buffer_puts(&b, S6_EXTBINPREFIX "s6-log -bd3 -- ") < 0) goto err ;
  if (stamptype & 1 && buffer_put(&b, "t ", 2) < 0) goto err ;
  if (stamptype & 2 && buffer_put(&b, "T ", 2) < 0) goto err ;
  if (buffer_put(&b, "n", 1) < 0
   || buffer_put(&b, fmt, uint_fmt(fmt, nfiles)) < 0
   || buffer_put(&b, " s", 2) < 0
   || buffer_put(&b, fmt, uint64_fmt(fmt, filesize)) < 0
   || buffer_put(&b, " ", 1) < 0) goto err ;
  if (maxsize)
  {
    if (buffer_put(&b, "S", 1) < 0
     || buffer_put(&b, fmt, uint64_fmt(fmt, maxsize)) < 0
     || buffer_put(&b, " ", 1) < 0) goto err ;
  }
  if (!string_quote(&sa, logdir, strlen(logdir))) strerr_diefu1sys(111, "quote string") ;
  if (buffer_put(&b, sa.s, sa.len) < 0 || buffer_put(&b, "\n", 1) < 0) goto err ;
  sa.len = 0 ;

  if (!buffer_flush(&b)) goto err ;
  fd_close(fd) ;
  return ;
 err:
  strerr_diefu2sys(111, "write to ", runfile) ;
}

static void write_service (char const *dir, char const *user, char const *sc, char const *logger, char const *path, char const *userenvdir, char const *const *vars, size_t varlen)
{
  size_t dirlen = strlen(dir) ;
  char fn[dirlen + 17] ;
  memcpy(fn, dir, dirlen) ;
  memcpy(fn + dirlen, "/notification-fd", 17) ;
  if (!openwritenclose_unsafe(fn, "3\n", 2)) strerr_diefu2sys(111, "write to ", fn) ;
  memcpy(fn + dirlen + 1, "run", 4) ;
  write_run(fn, user, sc, path, userenvdir, vars, varlen) ;
  if (logger)
  {
    struct iovec v[2] = { { .iov_base = (char *)logger, .iov_len = strlen(logger) }, { .iov_base = "\n", .iov_len = 1 } } ;
    memcpy(fn + dirlen + 1, "type", 5) ;
    if (!openwritenclose_unsafe(fn, "longrun\n", 8)) strerr_diefu2sys(111, "write to ", fn) ;
    memcpy(fn + dirlen + 1, "producer-for", 13) ;
    if (!openwritevnclose_unsafe(fn, v, 2)) strerr_diefu2sys(111, "write to ", fn) ;
  }
  else
  {
    if (chmod(fn, mask | ((mask >> 2) & 0111)) < 0)
      strerr_diefu2sys(111, "chmod ", fn) ;
  }
}

static void write_logger (char const *dir, char const *user, char const *logdir, unsigned int stamptype, unsigned int nfiles, uint64_t filesize, uint64_t maxsize, char const *service, char const *pipelinename)
{
  size_t dirlen = strlen(dir) ;
  char fn[dirlen + 17] ;
  memcpy(fn, dir, dirlen) ;
  memcpy(fn + dirlen, "/notification-fd", 17) ;
  if (!openwritenclose_unsafe(fn, "3\n", 2)) goto err ;
  memcpy(fn + dirlen + 1, "run", 4) ;
  write_logrun(fn, user, logdir, stamptype, nfiles, filesize, maxsize) ;
  if (service)
  {
    struct iovec v[2] = { { .iov_base = (char *)service, .iov_len = strlen(service) }, { .iov_base = "\n", .iov_len = 1 } } ;
    memcpy(fn + dirlen + 1, "type", 5) ;
    if (!openwritenclose_unsafe(fn, "longrun\n", 8)) goto err ;
    memcpy(fn + dirlen + 1, "consumer-for", 13) ;
    if (!openwritevnclose_unsafe(fn, v, 2)) goto err ;
    if (pipelinename)
    {
      v[0].iov_base = (char *)pipelinename ;
      v[0].iov_len = strlen(pipelinename) ;
      memcpy(fn + dirlen + 1, "pipeline-name", 14) ;
      if (!openwritevnclose_unsafe(fn, v, 2)) goto err ;
    }
  }
  else
  {
    if (chmod(fn, mask | ((mask >> 2) & 0111)) < 0)
      strerr_diefu2sys(111, "chmod ", fn) ;
  }
  return ;
 err:
  strerr_diefu2sys(111, "write to ", fn) ;
}

int main (int argc, char *const *argv)
{
  char const *userscandir = "${HOME}/service" ;
  char const *path = SKALIBS_DEFAULTPATH ;
  char const *userenvdir = 0 ;
  char *rcinfo[3] = { 0, 0, 0 } ;
  char const *loguser = 0 ;
  unsigned int stamptype = 1 ;
  unsigned int nfiles = 10 ;
  uint64_t filesize = 1000000 ;
  uint64_t maxsize = 0 ;
  size_t dirlen ;
  size_t varlen = 0 ;
  char const *vars[VARS_MAX] ;
  PROG = "s6-usertree-maker" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, (char const *const *)argv, "d:p:E:e:r:l:t:n:s:S:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'd' : userscandir = l.arg ; break ;
        case 'p' : path = l.arg ; break ;
        case 'E' : userenvdir = l.arg ; break ;
        case 'e' :
          if (varlen >= VARS_MAX) strerr_dief1x(100, "too many -v variables") ;
          if (strchr(l.arg, '=')) strerr_dief2x(100, "invalid variable name: ", l.arg) ;
          vars[varlen++] = l.arg ;
          break ;
        case 'r' : rcinfo[0] = (char *)l.arg ; break ;
        case 'l' : loguser = l.arg ; break ;
        case 't' : if (!uint0_scan(l.arg, &stamptype)) dieusage() ; break ;
        case 'n' : if (!uint0_scan(l.arg, &nfiles)) dieusage() ; break ;
        case 's' : if (!uint640_scan(l.arg, &filesize)) dieusage() ; break ;
        case 'S' : if (!uint640_scan(l.arg, &maxsize)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (argc < 3) dieusage() ;
  if (argv[1][0] != '/') strerr_dief1x(100, "logdir must be absolute") ;
  if (userscandir[0] != '/' && !str_start(userscandir, "${HOME}/"))
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
  mask = umask(0) ;
  mask = ~mask & 0666 ;
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
    memcpy(dir + dirlen + 1, rcinfo[0], svclen + 1) ;
    if (mkdir(dir, 0755) < 0) strerr_diefu2sys(111, "mkdir ", dir) ;
    memcpy(dir + dirlen + 1, rcinfo[1], loglen + 1) ;
    if (mkdir(dir, 0755) < 0) strerr_diefu2sys(111, "mkdir ", dir) ;
    umask(mask) ;
    write_logger(dir, loguser, argv[1], stamptype, nfiles, filesize, maxsize, rcinfo[0], rcinfo[2]) ;
    memcpy(dir + dirlen + 1, rcinfo[0], svclen + 1) ;
    write_service(dir, argv[0], userscandir, rcinfo[1], path, userenvdir, vars, varlen) ;
  }
  else
  {
    char dir[dirlen + 5] ;
    memcpy(dir, argv[2], dirlen) ;
    memcpy(dir + dirlen, "/log", 5) ;
    if (mkdir(argv[2], 0755) < 0) strerr_diefu2sys(111, "mkdir ", argv[2]) ;
    if (mkdir(dir, 0755) < 0) strerr_diefu2sys(111, "mkdir ", argv[2]) ;
    umask(mask) ;
    write_service(argv[2], argv[0], userscandir, 0, path, userenvdir, vars, varlen) ;
    write_logger(dir, loguser, argv[1], stamptype, nfiles, filesize, maxsize, 0, 0) ;
  }
  return 0 ;
}
