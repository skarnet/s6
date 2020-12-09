/* ISC license. */

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include <skalibs/gccattributes.h>
#include <skalibs/types.h>
#include <skalibs/strerr2.h>
#include <skalibs/sgetopt.h>
#include <skalibs/cdb.h>
#include <skalibs/djbunix.h>
#include <skalibs/socket.h>
#include <skalibs/exec.h>

#include <s6/config.h>
#include <s6/accessrules.h>

#ifdef S6_USE_EXECLINE
#include <execline/config.h>
#endif

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

static void logit (pid_t pid, uid_t uid, gid_t gid, int h)
{
  char fmtpid[PID_FMT] ;
  char fmtuid[UID_FMT] ;
  char fmtgid[GID_FMT] ;
  fmtpid[pid_fmt(fmtpid, pid)] = 0 ;
  fmtuid[uid_fmt(fmtuid, uid)] = 0 ;
  fmtgid[gid_fmt(fmtgid, gid)] = 0 ;
  if (h) strerr_warni7x("allow", " pid ", fmtpid, " uid ", fmtuid, " gid ", fmtgid) ;
  else strerr_warni7sys("deny", " pid ", fmtpid, " uid ", fmtuid, " gid ", fmtgid) ;
}

static inline void log_accept (pid_t pid, uid_t uid, gid_t gid)
{
  logit(pid, uid, gid, 1) ;
}

static inline void log_deny (pid_t pid, uid_t uid, gid_t gid)
{
  logit(pid, uid, gid, 0) ;
}


 /* Checking */

static s6_accessrules_result_t check_cdb (uid_t uid, gid_t gid, char const *file, s6_accessrules_params_t *params)
{
  struct cdb c = CDB_ZERO ;
  int fd = open_readb(file) ;
  s6_accessrules_result_t r ;
  if (fd < 0) return -1 ;
  if (cdb_init(&c, fd) < 0) strerr_diefu2sys(111, "cdb_init ", file) ;
  r = s6_accessrules_uidgid_cdb(uid, gid, &c, params) ;
  cdb_free(&c) ;
  fd_close(fd) ;
  return r ;
}

static inline int check (s6_accessrules_params_t *params, char const *rules, unsigned int rulestype, uid_t uid, gid_t gid)
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

int main (int argc, char const *const *argv)
{
  s6_accessrules_params_t params = S6_ACCESSRULES_PARAMS_ZERO ;
  char const *rules = 0 ;
  char const *localname = 0 ;
  char const *proto ;
  size_t protolen ;
  uid_t uid = 0 ;
  gid_t gid = 0 ;
  unsigned int rulestype = 0 ;
  int doenv = 1 ;
  PROG = "s6-ipcserver-access" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "v:Eel:i:x:", &l) ;
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

  proto = getenv("PROTO") ;
  if (!proto) strerr_dienotset(100, "PROTO") ;
  protolen = strlen(proto) ;

  {
    char const *x ;
    char tmp[protolen + 11] ;
    memcpy(tmp, proto, protolen) ;
    memcpy(tmp + protolen, "REMOTEEUID", 11) ;
    x = getenv(tmp) ;
    if (!x) strerr_dienotset(100, tmp) ;
    if (!uid0_scan(x, &uid)) strerr_dieinvalid(100, tmp) ;
    tmp[protolen + 7] = 'G' ;
    x = getenv(tmp) ;
    if (!x) strerr_dienotset(100, tmp) ;
    if (!gid0_scan(x, &gid)) strerr_dieinvalid(100, tmp) ;
  }

  if (check(&params, rules, rulestype, uid, gid)) goto accepted ;

  if (verbosity >= 2) log_deny(getpid(), uid, gid) ;
  return 1 ;

 accepted:
  if (verbosity) log_accept(getpid(), uid, gid) ;

  if (doenv)
  {
    char tmp[protolen + 10] ;
    memcpy(tmp, proto, protolen) ;
    memcpy(tmp + protolen, "LOCALPATH", 10) ;
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
    memcpy(tmp, proto, protolen) ;
    memcpy(tmp + protolen, "REMOTEEUID", 11) ;
    if (!env_addmodif(&params.env, "PROTO", 0)) dienomem() ;
    if (!env_addmodif(&params.env, tmp, 0)) dienomem() ;
    tmp[protolen + 7] = 'G' ;
    if (!env_addmodif(&params.env, tmp, 0)) dienomem() ;
    memcpy(tmp + protolen + 6, "PATH", 5) ;
    if (!env_addmodif(&params.env, tmp, 0)) dienomem() ;
    memcpy(tmp + protolen, "LOCALPATH", 10) ;
    if (!env_addmodif(&params.env, tmp, 0)) dienomem() ;
    memcpy(tmp + protolen, "CONNNUM", 8) ;
    if (!env_addmodif(&params.env, tmp, 0)) dienomem() ;
  }

  if (params.exec.len)
#ifdef S6_USE_EXECLINE
  {
    char *specialargv[4] = { EXECLINE_EXTBINPREFIX "execlineb", "-Pc", params.exec.s, 0 } ;
    xmexec_m((char const *const *)specialargv, params.env.s, params.env.len) ;
  }
#else
  strerr_warnw1x("exec file found but ignored because s6 was compiled without execline support!") ;
#endif
  xmexec_m(argv, params.env.s, params.env.len) ;
}
