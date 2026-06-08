/* ISC license. */

#include <skalibs/bsdsnowflake.h>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pwd.h>

#include <skalibs/types.h>
#include <skalibs/posixplz.h>
#include <skalibs/stat.h>
#include <skalibs/envexec.h>
#include <skalibs/alloc.h>
#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/skamisc.h>

#include <execline/config.h>

#include <s6/config.h>

#define USAGE "s6-svscanboot [ -c devconsole ] [ -D catchalldir ] [ -m catchallmode ] [ -l catchalluser ] [ -o catchalloptions ] [ -d notif ] [ -C services_max ] [ -L name_max ] [ -t rescan ] scandir"
#define dieusage() strerr_dieusage(100, USAGE)

enum golb_e
{
  GOLB_USER = 0x01,
} ;

enum gola_e
{
  GOLA_CONSOLE,
  GOLA_DIR,
  GOLA_MODE,
  GOLA_USER,
  GOLA_OPTIONS,
  GOLA_NOTIF,
  GOLA_MAXSERVICES,
  GOLA_MAXLEN,
  GOLA_TIMEOUT,
  GOLA_N
} ;

int main (int argc, char const *const *argv)
{
  static gol_bool const rgolb[] =
  {
    { .so = 'u', .lo = "user", .clear = 0, .set = GOLB_USER },
  } ;
  static gol_arg const rgola[] =
  {
    { .so = 'c', .lo = "console", .i = GOLA_CONSOLE },
    { .so = 'D', .lo = "catchall-directory", .i = GOLA_DIR },
    { .so = 'm', .lo = "catchall-mode", .i = GOLA_MODE },
    { .so = 'l', .lo = "catchall-user", .i = GOLA_USER },
    { .so = 'o', .lo = "catchall-options", .i = GOLA_OPTIONS },
    { .so = 'd', .lo = "notification-fd", .i = GOLA_NOTIF },
    { .so = 'C', .lo = "services-max", .i = GOLA_MAXSERVICES },
    { .so = 'L', .lo = "name-max", .i = GOLA_MAXLEN },
    { .so = 't', .lo = "timeout", .i = GOLA_TIMEOUT },
  } ;
  unsigned int catchall_mode = 02750 ;
  unsigned int fdconsole = 3 ;
  unsigned int fdnotif = 0 ;
  unsigned int servicesmax = 0 ;
  unsigned int namemax = 0 ;
  unsigned int timeout = 0 ;
  uid_t uid = -1 ;
  gid_t gid = -1 ;
  stralloc sa = STRALLOC_ZERO ;
  buffer b ;
  int fd ;
  unsigned int m = 0 ;
  char const *newargv[14] ;
  uint64_t wgolb = 0 ;
  char const *wgola[GOLA_N] = { 0 } ;
  unsigned int golc ;
  char buf[4096] ;
  char fmtc[UINT_FMT] ;
  char fmtd[UINT_FMT] ;
  char fmtC[UINT_FMT] ;
  char fmtL[UINT_FMT] ;
  char fmtt[UINT_FMT] ;

  PROG = "s6-svscanboot" ;
  wgola[GOLA_OPTIONS] = "t" ;

  golc = GOL_main(argc, argv, rgolb, rgola, &wgolb, wgola) ;
  argc -= golc ; argv += golc ;
  if (!argc) dieusage() ;

  if (wgola[GOLA_MODE] && !uint0_oscan(wgola[GOLA_MODE], &catchall_mode))
    strerr_dief(100, "catchall-mode must be an (octal) unsigned integer") ;
  if (wgola[GOLA_NOTIF])
  {
    if (!uint0_scan(wgola[GOLA_NOTIF], &fdnotif))
      strerr_dief(100, "notification-fd", " must be an unsigned integer") ;
    if (fdnotif < 3)
      strerr_dief(100, "notification-fd", " must be 3 or greater") ;
    if (fcntl(fdnotif, F_GETFD) == -1)
      strerr_diefsys(111, "notification-fd", " sanity check failed") ;
    if (fdnotif == 3) fdconsole = 4 ;
  }
  if (wgola[GOLA_MAXSERVICES])
  {
    if (!uint0_scan(wgola[GOLA_MAXSERVICES], &servicesmax))
      strerr_dief(100, "services-max", " must be an unsigned integer") ;
    if (servicesmax < 4) servicesmax = 4 ;
    if (servicesmax > 160000) servicesmax = 160000 ;
  }
  if (wgola[GOLA_MAXLEN])
  {
    if (!uint0_scan(wgola[GOLA_MAXLEN], &namemax))
      strerr_dief(100, "name-max", " must be an unsigned integer") ;
    if (namemax < 11) namemax = 11 ;
    if (namemax > 1019) namemax = 1019 ;
  }
  if (wgola[GOLA_TIMEOUT] && !uint0_scan(wgola[GOLA_TIMEOUT], &timeout))
    strerr_dief(100, "timeout", " must be an unsigned integer") ;
  if (wgola[GOLA_USER] && geteuid())
  {
    wgola[GOLA_USER] = 0 ;
    strerr_warnw("ignoring catchall-user option when not root") ;
  }
  if (wgolb & GOLB_USER && (!wgola[GOLA_DIR] || wgola[GOLA_DIR][0] != '/'))
  {
    size_t namelen ;
    size_t dirlen ;
    char *logdir ;
    char *x = getenv("XDG_RUNTIME_DIR") ;
    if (!x) strerr_dienotset(100, "XDG_RUNTIME_DIR") ;
    if (!wgola[GOLA_DIR]) wgola[GOLA_DIR] = "uncaught-logs" ;
    dirlen = strlen(x) ;
    namelen = strlen(wgola[GOLA_DIR]) ;
    logdir = alloc(dirlen + namelen + 2) ;
    if (!logdir) strerr_diefusys(111, "alloc") ;
    memcpy(logdir, x, dirlen) ;
    logdir[dirlen] = '/' ;
    memcpy(logdir + dirlen + 1, wgola[GOLA_DIR], namelen + 1) ;
    wgola[GOLA_DIR] = logdir ;
  }
  if (!wgola[GOLA_DIR]) wgola[GOLA_DIR] = "/run/uncaught-logs" ;
  if (wgola[GOLA_USER])
  {
    struct passwd *pw = getpwnam(wgola[GOLA_USER]) ;
    if (!pw) strerr_diefsys(111, "getpwnam ", wgola[GOLA_USER]) ;
    uid = pw->pw_uid ;
    gid = pw->pw_gid ;
  }

  if (mkdirp(wgola[GOLA_DIR], catchall_mode) == -1)
    strerr_diefusys(111, "mkdir ", wgola[GOLA_DIR]) ;
  if (chown(wgola[GOLA_DIR], uid, gid) == -1)
    strerr_diefusys(111, "chown ", wgola[GOLA_DIR]) ;
  if (chmod(wgola[GOLA_DIR], catchall_mode) == -1)
    strerr_diefusys(111, "chmod ", wgola[GOLA_DIR]) ;
  if (wgola[GOLA_DIR][0] != '/')
  {
    char *x = realpath(wgola[GOLA_DIR], 0) ;
    if (!x) strerr_diefusys(111, "realpath ", wgola[GOLA_DIR]) ;
    wgola[GOLA_DIR] = x ;
  }

  size_t slen = strlen(argv[0]) ;
  char fn[slen + 31] ;
  memcpy(fn, argv[0], slen) ;
  memcpy(fn + slen, "/s6-svscan-log", 15) ;
  rm_rf(fn) ;
  if (mkdirp2(fn, 02755) == -1)
    strerr_diefusys(111, "mkdir ", fn) ;

  memcpy(fn + slen + 14, "/notification-fd", 17) ;
  if (!openwritenclose_unsafe(fn, "3\n", 2))
    strerr_diefusys(111, "write to ", fn) ;

  memcpy(fn + slen + 14, "/run", 5) ;
  fd = openc_create(fn) ;
  buffer_init(&b, &buffer_write, fd, buf, 4096) ;

#define diew() strerr_diefusys(111, "write to ", fn) ;

  if (buffer_puts(&b,
    "#!" EXECLINE_SHEBANGPREFIX "execlineb -s1\n"
    EXECLINE_EXTBINPREFIX "fdmove -c 1 2\n"
    EXECLINE_EXTBINPREFIX "redirfd -rnb 0 fifo\n") == -1) diew() ;
  if (wgola[GOLA_USER])
  {
    if (!string_quotes(&sa, wgola[GOLA_USER]))
      strerr_diefusys(111, "quote ", "catchall-user") ;
    if (buffer_puts(&b, S6_EXTBINPREFIX "s6-setuidgid ") == -1
     || buffer_put(&b, sa.s, sa.len) == -1
     || buffer_put(&b, "\n", 1) == -1) diew() ;
    sa.len = 0 ;
  }
  if (!string_quotes(&sa, wgola[GOLA_DIR]))
    strerr_diefusys(111, "quote ", "catchall-directory") ;
  if (buffer_puts(&b,
    EXECLINE_EXTBINPREFIX "exec -c\n"
    S6_EXTBINPREFIX "s6-log -bpd3 -- ") == -1
   || buffer_puts(&b, wgola[GOLA_OPTIONS]) == -1
   || buffer_put(&b, " ", 1) == -1
   || buffer_put(&b, sa.s, sa.len) == -1
   || buffer_putflush(&b, "\n", 1) == -1) diew() ;
  sa.len = 0 ;
  if (fchmod(fd, 0755) == -1)
    strerr_diefusys(111, "chmod ", fn) ;
  fd_close(fd) ;

  memcpy(fn + slen + 14, "/fifo", 6) ;
  if (mkfifo(fn, 0600) == -1)
    strerr_diefusys(111, "mkfifo ", fn) ;

  fd = open2("/dev/null", O_RDONLY) ;
  if (fd == -1) strerr_diefusys(111, "open ", "/dev/null") ;
  if (fd_move(0, fd) == -1) strerr_diefusys(111, "fd_move ", "/dev/null", " to ", "stdin") ;
  if (wgola[GOLA_CONSOLE])
  {
    fd = open2(wgola[GOLA_CONSOLE], O_WRONLY) ;
    if (fd == -1) strerr_diefusys(111, "open ", wgola[GOLA_CONSOLE]) ;
    if (fd_move(fdconsole, fd) == -1) strerr_diefusys(111, "fd_move ", wgola[GOLA_CONSOLE]) ;
  }
  else if (fd_copy(fdconsole, 2) == -1) strerr_diefusys(111, "fd_copy ", "stderr") ;
  b.fd = open2(fn, O_RDONLY | O_NONBLOCK) ;
  if (b.fd == -1) strerr_diefusys(111, "open ", fn, " for reading") ;
  fd = open2(fn, O_WRONLY | O_NONBLOCK) ;
  if (fd == -1) strerr_diefusys(111, "open ", fn, " for writing") ;
  fd_close(b.fd) ;
  if (ndelay_off(fd) == -1) strerr_diefusys(111, "ndelay_off ", fn) ;
  if (fd_move(1, fd) == -1) strerr_diefusys(111, "fd_move ", fn, " to ", "stdout") ;
  if (fd_copy(2, 1) == -1) strerr_diefusys(111, "fd_copy ", "stdout", " to ", "stderr") ;

  fmtc[uint_fmt(fmtc, fdconsole)] = 0 ;

  newargv[m++] = S6_BINPREFIX "s6-svscan" ;
  newargv[m++] = "-X" ;
  newargv[m++] = fmtc ;
  if (fdnotif)
  {
    fmtd[uint_fmt(fmtd, fdnotif)] = 0 ;
    newargv[m++] = "-d" ;
    newargv[m++] = fmtd ;
  }
  if (servicesmax)
  {
    fmtC[uint_fmt(fmtC, servicesmax)] = 0 ;
    newargv[m++] = "-C" ;
    newargv[m++] = fmtC ;
  }
  if (namemax)
  {
    fmtL[uint_fmt(fmtL, namemax)] = 0 ;
    newargv[m++] = "-L" ;
    newargv[m++] = fmtL ;
  }
  if (timeout)
  {
    fmtt[uint_fmt(fmtt, timeout)] = 0 ;
    newargv[m++] = "-t" ;
    newargv[m++] = fmtt ;
  }
  newargv[m++] = "--" ;
  newargv[m++] = argv[0] ;
  newargv[m++] = 0 ;

  exec(newargv) ;
  if (fd_move(2, fdconsole) == -1) _exit(111) ;
  strerr_diefusys(111, "exec ", newargv[0]) ;
}
