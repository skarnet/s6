/* ISC license. */

#include <unistd.h>
#include <errno.h>
#include <skalibs/gccattributes.h>
#include <skalibs/uint.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/cdb.h>
#include <skalibs/env.h>
#include <skalibs/djbunix.h>
#include <skalibs/webipc.h>
#include <execline/config.h>
#include <s6/accessrules.h>

#define USAGE "s6-ipcserver-access [ -v verbosity ] [ -e | -E ] [ -l localname ] [ -i rulesdir | -x rulesfile ] prog..."

static unsigned int verbosity = 1 ;

 /* Utility functions */

static inline void dieusage (void) gccattr_noreturn ;
static inline void dieusage ()
{
  strerr_dieusage(100, USAGE) ;
}

static inline void dienomem (void) gccattr_noreturn ;
static inline void dienomem ()
{
  strerr_diefu1sys(111, "update environment") ;
}

static inline void X (void) gccattr_noreturn ;
static inline void X ()
{
  strerr_dief1x(101, "internal inconsistency. Please submit a bug-report.") ;
}


 /* Logging */

static void logit (unsigned int pid, unsigned int uid, unsigned int gid, int h)
{
  char fmtpid[UINT_FMT] ;
  char fmtuid[UINT_FMT] ;
  char fmtgid[UINT_FMT] ;
  fmtpid[uint_fmt(fmtpid, pid)] = 0 ;
  fmtuid[uint_fmt(fmtuid, uid)] = 0 ;
  fmtgid[uint_fmt(fmtgid, gid)] = 0 ;
  if (h) strerr_warni7x("allow", " pid ", fmtpid, " uid ", fmtuid, " gid ", fmtgid) ;
  else strerr_warni7sys("deny", " pid ", fmtpid, " uid ", fmtuid, " gid ", fmtgid) ;
}

static inline void log_accept (unsigned int pid, unsigned int uid, unsigned int gid)
{
  logit(pid, uid, gid, 1) ;
}

static inline void log_deny (unsigned int pid, unsigned int uid, unsigned int gid)
{
  logit(pid, uid, gid, 0) ;
}


 /* Checking */

static s6_accessrules_result_t check_cdb (unsigned int uid, unsigned int gid, char const *file, s6_accessrules_params_t *params)
{
  struct cdb c = CDB_ZERO ;
  int fd = open_readb(file) ;
  register s6_accessrules_result_t r ;
  if (fd < 0) return -1 ;
  if (cdb_init(&c, fd) < 0) strerr_diefu2sys(111, "cdb_init ", file) ;
  r = s6_accessrules_uidgid_cdb(uid, gid, &c, params) ;
  cdb_free(&c) ;
  fd_close(fd) ;
  return r ;
}

static inline int check (s6_accessrules_params_t *params, char const *rules, unsigned int rulestype, unsigned int uid, unsigned int gid)
{
  char const *x = "" ;
  s6_accessrules_result_t r ;
  switch (rulestype)
  {
    case 0 :
      if (verbosity >= 2) strerr_warnw1x("invoked without a ruleset!") ;
      return 1 ;
    case 1 :
      r = s6_accessrules_uidgid_fs(uid, gid, rules, params) ;
      x = "fs" ;
      break ;
    case 2 :
      r = check_cdb(uid, gid, rules, params) ;
      x = "cdb" ;
      break ;
    default : X() ;
  }
  switch (r)
  {
    case S6_ACCESSRULES_ERROR : strerr_diefu4sys(111, "check ", x, " ruleset in ", rules) ;
    case S6_ACCESSRULES_ALLOW :    return 1 ;
    case S6_ACCESSRULES_DENY :     return (errno = EACCES, 0) ;
    case S6_ACCESSRULES_NOTFOUND : return (errno = ENOENT, 0) ;
    default : X() ;
  }
}


int main (int argc, char const *const *argv, char const *const *envp)
{
  s6_accessrules_params_t params = S6_ACCESSRULES_PARAMS_ZERO ;
  char const *rules = 0 ;
  char const *localname = 0 ;
  char const *proto ;
  unsigned int protolen ;
  unsigned int uid = 0, gid = 0 ;
  unsigned int rulestype = 0 ;
  int doenv = 1 ;
  PROG = "s6-ipcserver-access" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "v:Eel:i:x:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'v' : if (!uint0_scan(l.arg, &verbosity)) dieusage() ; break ;
        case 'E' : doenv = 0 ; break ;
        case 'e' : doenv = 1 ; break ;
        case 'l' : localname = l.arg ; break ;
        case 'i' : rules = l.arg ; rulestype = 1 ; break ;
        case 'x' : rules = l.arg ; rulestype = 2 ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) dieusage() ;
  if (!*argv[0]) dieusage() ;

  proto = env_get2(envp, "PROTO") ;
  if (!proto) strerr_dienotset(100, "PROTO") ;
  protolen = str_len(proto) ;

  {
    char const *x ;
    char tmp[protolen + 11] ;
    byte_copy(tmp, protolen, proto) ;
    byte_copy(tmp + protolen, 11, "REMOTEEUID") ;
    x = env_get2(envp, tmp) ;
    if (!x) strerr_dienotset(100, tmp) ;
    if (!uint0_scan(x, &uid)) strerr_dieinvalid(100, tmp) ;
    tmp[protolen + 7] = 'G' ;
    x = env_get2(envp, tmp) ;
    if (!x) strerr_dienotset(100, tmp) ;
    if (!uint0_scan(x, &gid)) strerr_dieinvalid(100, tmp) ;
  }

  if (!check(&params, rules, rulestype, uid, gid))
  {
    if (verbosity >= 2) log_deny(getpid(), uid, gid) ;
    return 1 ;
  }
  if (verbosity) log_accept(getpid(), uid, gid) ;

  if (doenv)
  {
    char tmp[protolen + 10] ;
    byte_copy(tmp, protolen, proto) ;
    byte_copy(tmp + protolen, 10, "LOCALPATH") ;
    if (localname)
    {
      if (!env_addmodif(&params.env, tmp, localname)) dienomem() ;
    }
    else
    {
      char curname[IPCPATH_MAX+1] ;
      int dummy ;
      if (ipc_local(0, curname, IPCPATH_MAX+1, &dummy) < 0)
        strerr_diefu1sys(111, "ipc_local") ;
      if (!env_addmodif(&params.env, tmp, curname)) dienomem() ;
    }
  }
  else
  {
    char tmp[protolen + 11] ;
    byte_copy(tmp, protolen, proto) ;
    byte_copy(tmp + protolen, 11, "REMOTEEUID") ;
    if (!env_addmodif(&params.env, "PROTO", 0)) dienomem() ;
    if (!env_addmodif(&params.env, tmp, 0)) dienomem() ;
    tmp[protolen + 7] = 'G' ;
    if (!env_addmodif(&params.env, tmp, 0)) dienomem() ;
    byte_copy(tmp + protolen + 6, 5, "PATH") ;
    if (!env_addmodif(&params.env, tmp, 0)) dienomem() ;
    byte_copy(tmp + protolen, 10, "LOCALPATH") ;
    if (!env_addmodif(&params.env, tmp, 0)) dienomem() ;
  }

  if (params.exec.len)
  {
    char *specialargv[4] = { EXECLINE_EXTBINPREFIX "execlineb", "-c", params.exec.s, 0 } ;
    pathexec_r((char const *const *)specialargv, envp, env_len(envp), params.env.s, params.env.len) ;
    strerr_dieexec(111, specialargv[0]) ;
  }

  pathexec_r(argv, envp, env_len(envp), params.env.s, params.env.len) ;
  strerr_dieexec(111, argv[0]) ;
}
