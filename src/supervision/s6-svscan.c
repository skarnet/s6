/* ISC license. */

#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include <skalibs/posixplz.h>
#include <skalibs/uint32.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/sgetopt.h>
#include <skalibs/types.h>
#include <skalibs/strerr.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/devino.h>
#include <skalibs/cspawn.h>
#include <skalibs/djbunix.h>
#include <skalibs/direntry.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/exec.h>
#include <skalibs/bitarray.h>
#include <skalibs/genset.h>
#include <skalibs/avltreen.h>

#include <s6/config.h>
#include <s6/supervise.h>

#include <skalibs/posixishard.h>

#define USAGE "s6-svscan [ -c services_max | -C services_max ] [ -L name_max ] [ -t timeout ] [ -d notif ] [ -X consoleholder ] [ dir ]"
#define dieusage() strerr_dieusage(100, USAGE)

#define CTL S6_SVSCAN_CTLDIR "/control"
#define LCK S6_SVSCAN_CTLDIR "/lock"
#define FINISH_PROG S6_SVSCAN_CTLDIR "/finish"
#define CRASH_PROG S6_SVSCAN_CTLDIR "/crash"
#define SIGNAL_PROG S6_SVSCAN_CTLDIR "/SIG"
#define SIGNAL_PROG_LEN (sizeof(SIGNAL_PROG) - 1)
#define SPECIAL_LOGGER_SERVICE "s6-svscan-log"

typedef struct service_s service, *service_ref ;
struct service_s
{
  devino devino ;
  pid_t pid ;
  tain start ;
  int p ;
  uint32_t peer ;
} ;

struct flags_s
{
  uint8_t cont : 1 ;
  uint8_t waitall : 1 ;
} ;

static unsigned int consoleholder = 0 ;
static struct flags_s flags = { .cont = 1, .waitall = 0 } ;

static uint32_t namemax = 251 ;
static char *names ;
#define NAME(i) (names + (i) * (namemax + 5))

static genset *services ;
#define SERVICE(i) genset_p(service, services, (i))
static uint32_t max = 1000 ;
static uint32_t special ;

static avltreen *by_pid ;
static avltreen *by_devino ;
static char *active ;

static tain scan_deadline = TAIN_EPOCH ;
static tain start_deadline = TAIN_INFINITE ;
static tain scantto = TAIN_INFINITE_RELATIVE ;


 /* Tree management */

static void *bydevino_dtok (uint32_t d, void *aux)
{
  genset *g = aux ;
  return &genset_p(service, g, d)->devino ;
}

static int bydevino_cmp (void const *a, void const *b, void *aux)
{
  (void)aux ;
  return devino_cmp(a, b) ;
}

static void *bypid_dtok (uint32_t d, void *aux)
{
  genset *g = aux ;
  return &genset_p(service, g, d)->pid ;
}

static int bypid_cmp (void const *a, void const *b, void *aux)
{
  (void)aux ;
  pid_t const *aa = a ;
  pid_t const *bb = b ;
  return *aa < *bb ? -1 : *aa > *bb ;
}


 /* On-exit utility */

static void restore_console (void)
{
  if (consoleholder)
  {
    fd_move(2, consoleholder) ;
    if (fd_copy(1, 2) < 0) strerr_warnwu1sys("restore stdout") ;
  }
}

static void panicnosp (char const *) gccattr_noreturn ;
static void panicnosp (char const *errmsg)
{
  char const *eargv[2] = { CRASH_PROG, 0 } ;
  strerr_warnwu1sys(errmsg) ;
  strerr_warnw2x("executing into ", eargv[0]) ;
  execv(eargv[0], (char *const *)eargv) ;
  strerr_dieexec(errno == ENOENT ? 127 : 126, eargv[0]) ;
}

static void panic (char const *) gccattr_noreturn ;
static void panic (char const *errmsg)
{
  int e = errno ;
  selfpipe_finish() ;
  restore_console() ;
  errno = e ;
  panicnosp(errmsg) ;
}

static int close_pipes_iter (void *data, void *aux)
{
  service *sv = data ;
  if (sv->p >= 0) close(sv->p) ;
  (void)aux ;
  return 1 ;
}

static inline void close_pipes (void)
{
  genset_iter(services, &close_pipes_iter, 0) ;
  if (special < max)
  {
    fd_close(1) ;
    if (open2("/dev/null", O_WRONLY) < 0)
      strerr_warnwu1sys("open /dev/null") ;
  }
}

static inline void waitthem (void)
{
  while (avltreen_len(by_pid))
  {
    int wstat ;
    pid_t pid = wait_nointr(&wstat) ;
    if (pid < 0)
    {
      strerr_warnwu1sys("wait for all s6-supervise processes") ;
      break ;
    }
    avltreen_delete(by_pid, &pid) ;
  }
}


 /* Misc utility */

 /*
    what:
     1 -> kill all services
     2 -> kill inactive services
     4 -> send a SIGTERM to loggers instead of SIGHUP
     8 -> reap
    16 -> delay killing until the next scan
 */

static inline int is_logger (uint32_t i)
{
  return !!strchr(NAME(i), '/') ;
}

 /* Triggered actions: config */

static inline void chld (unsigned int *what)
{
  *what |= 8 ;
}

static inline void alrm (unsigned int *what)
{
  *what |= 16 ;
  tain_copynow(&scan_deadline) ;
}

static inline void abrt (void)
{
  flags.cont = 0 ;
  flags.waitall = 0 ;
}

static void hup (unsigned int *what)
{
  *what |= 18 ;
  tain_copynow(&scan_deadline) ;
}

static void term (unsigned int *what)
{
  flags.cont = 0 ;
  flags.waitall = 1 ;
  *what |= 3 ;
}

static void quit (unsigned int *what)
{
  flags.cont = 0 ;
  flags.waitall = 1 ;
  *what |= 7 ;
}

static void handle_signals (unsigned int *what)
{
  for (;;)
  {
    int sig = selfpipe_read() ;
    switch (sig)
    {
      case -1 : panic("selfpipe_read") ;
      case 0 : return ;
      case SIGCHLD : chld(what) ; break ;
      case SIGALRM : alrm(what) ; break ;
      case SIGABRT : abrt() ; break ;
      default :
      {
        int usebuiltin = 1 ;
        char const *name = sig_name(sig) ;
        size_t len = strlen(name) ;
        char fn[SIGNAL_PROG_LEN + len + 1] ;
        char const *const newargv[2] = { fn, 0 } ;
        memcpy(fn, SIGNAL_PROG, SIGNAL_PROG_LEN) ;
        memcpy(fn + SIGNAL_PROG_LEN, name, len + 1) ;
        if (access(newargv[0], X_OK) == 0)  /* avoids a spawn, don't care about the toctou */
        {
          if (cspawn(newargv[0], newargv, (char const **)environ, CSPAWN_FLAGS_SELFPIPE_FINISH, 0, 0))
            usebuiltin = 0 ;
          else if (errno != ENOENT) strerr_warnwu2sys("spawn ", newargv[0]) ;
        }
        if (usebuiltin) switch (sig)
        {
          case SIGHUP : hup(what) ; break ;
          case SIGINT :
          case SIGTERM : term(what) ; break ;
          case SIGQUIT : quit(what) ; break ;
        }
      }
    }
  }
}

static void handle_control (int fd, unsigned int *what)
{
  for (;;)
  {
    char c ;
    ssize_t r = sanitize_read(fd_read(fd, &c, 1)) ;
    if (r < 0) panic("read control pipe") ;
    else if (!r) break ;
    switch (c)
    {
      case 'z' : chld(what) ; break ;
      case 'a' : alrm(what) ; break ;
      case 'b' : abrt() ; break ;
      case 'h' : hup(what) ; break ;
      case 'i' :
      case 't' : term(what) ; break ;
      case 'q' : quit(what) ; break ;
      case 'n' : *what |= 2 ; break ;
      case 'N' : *what |= 6 ; break ;
      default :
      {
        char s[2] = { c, 0 } ;
        strerr_warnw2x("received unknown control command: ", s) ;
      }
    }
  }
}


 /* Triggered action: killer */

static int killthem_iter (void *data, void *aux)
{
  service *sv = data ;
  unsigned int *what = aux ;
  uint32_t i = sv - SERVICE(0) ;
  if ((*what & 1 || !bitarray_peek(active, i)) && sv->pid)
    kill(sv->pid, *what & (2 << (i == special || is_logger(i))) ? SIGTERM : SIGHUP) ;
  return 1 ;
}

static inline void killthem (unsigned int *what)
{
  if (*what & 16 || !(*what & 7)) return ;
  genset_iter(services, &killthem_iter, what) ;
  *what &= ~7 ;
}


 /* Triggered action: reaper */

 /*
   sv->p values:
   0+ : this end of the pipe
   -1 : not a logged service
   -2 : inactive and peer dead, do not reactivate
   -3 : reactivation wanted, trigger rescan on death
 */

static void remove_service (service *sv)
{
  if (sv->peer < max)
  {
    service *peer = SERVICE(sv->peer) ;
    if (peer->p >= 0)
    {
      close(peer->p) ;
      peer->p = -2 ;
    }
    peer->peer = max ;
  }
  if (sv->p == -3) tain_earliest1(&scan_deadline, &sv->start) ;
  else if (sv->p >= 0) close(sv->p) ;
  avltreen_delete(by_devino, &sv->devino) ;
  genset_delete(services, sv - SERVICE(0)) ;
}

static void reap (unsigned int *what)
{
  if (!(*what & 8)) return ;
  *what &= ~8 ;
  for (;;)
  {
    uint32_t i ;
    int wstat ;
    pid_t pid = wait_nohang(&wstat) ;
    if (pid < 0)
      if (errno != ECHILD) panic("wait_nohang") ;
      else break ;
    else if (!pid) break ;
    else if (avltreen_search(by_pid, &pid, &i))
     {
      service *sv = SERVICE(i) ;
      avltreen_delete(by_pid, &pid) ;
      sv->pid = 0 ;
      if (bitarray_peek(active, i)) tain_earliest1(&start_deadline, &sv->start) ;
      else remove_service(sv) ;
    }
  }
}


 /*
    On-timeout action: scanner.
    (This can be triggered, but the trigger just sets the timeout to 0.)
    It's on-timeout because it can fail and get rescheduled for later.
 */

static int check (char const *name, uint32_t prod, char *act)
{
  struct stat st ;
  devino di ;
  uint32_t i ;
  service *sv ;
  if (stat(name, &st) == -1)
  {
    if (prod < max && errno == ENOENT)
    {
      if (SERVICE(prod)->peer < max)
        strerr_warnw3x("logger for service ", NAME(prod), " has been moved") ;
      return max ;
    }
    strerr_warnwu2sys("stat ", name) ;
    return -4 ;
  }
  if (!S_ISDIR(st.st_mode)) return max ;
  di.dev = st.st_dev ;
  di.ino = st.st_ino ;
  if (avltreen_search(by_devino, &di, &i))
  {
    sv = SERVICE(i) ;
    if (sv->peer < max)
    {
      if (prod < max && prod != sv->peer)
      {
        strerr_warnw3x("old service ", name, " still exists, waiting") ;
        return -10 ;
      }
      if (sv->p == -1)
      {
        sv->p = -2 ;
        return i ;
      }
    }
  }
  else
  {
    i = genset_new(services) ;
    if (i >= max)
    {
      strerr_warnwu3x("start supervisor for ", name, ": too many services") ;
      return -60 ;
    }
    sv = SERVICE(i) ;
    sv->devino = di ;
    sv->pid = 0 ;
    tain_copynow(&sv->start) ;
    tain_copynow(&start_deadline) ; /* XXX: may cause a superfluous start if logger fails, oh well */
    if (prod >= max)
    {
      sv->peer = max ;
      sv->p = -1 ;
      if (special >= max && !strcmp(name, SPECIAL_LOGGER_SERVICE)) special = i ;
    }
    else
    {
      int p[2] ;
      if (pipecoe(p) == -1)
      {
        strerr_warnwu2sys("create pipe for ", name) ;
        genset_delete(services, i) ;
        return -3 ;
      }
      sv->peer = prod ;
      sv->p = p[0] ;
      SERVICE(prod)->peer = i ;
      SERVICE(prod)->p = p[1] ;
    }
    avltreen_insert(by_devino, i) ;
  }
  strcpy(NAME(i), name) ;
  bitarray_set(act, i) ;
  return i ;
}

static void set_scan_timeout (unsigned int n)
{
  tain a ;
  tain_addsec_g(&a, n) ;
  tain_earliest1(&scan_deadline, &a) ;
}

static int remove_deadinactive_iter (void *data, void *aux)
{
  service *sv = data ;
  uint32_t *n = aux ;
  if (!bitarray_peek(active, sv - SERVICE(0)))
  {
    if (!sv->pid) remove_service(sv) ;
    if (!--n) return 0 ;
  }
  return 1 ;
}

static void scan (unsigned int *what)
{
  DIR *dir = opendir(".") ;
  char tmpactive[bitarray_div8(max)] ;
  tain_add_g(&scan_deadline, &scantto) ;
  memset(tmpactive, 0, bitarray_div8(max)) ;
  if (!dir)
  {
    strerr_warnwu1sys("opendir .") ;
    set_scan_timeout(5) ;
    return ;
  }
  for (;;)
  {
    int i ;
    size_t len ;
    direntry *d ;
    errno = 0 ;
    d = readdir(dir) ;
    if (!d) break ;
    if (d->d_name[0] == '.') continue ;
    len = strlen(d->d_name) ;
    if (len > namemax)
    {
      strerr_warnw2x("name too long - not spawning service: ", d->d_name) ;
      continue ;
    }
    i = check(d->d_name, max, tmpactive) ;
    if (i < 0)
    {
      dir_close(dir) ;
      set_scan_timeout(-i) ;
      return ;
    }
    if (i < max)
    {
      char logname[len + 5] ;
      memcpy(logname, d->d_name, len) ;
      memcpy(logname + len, "/log", 5) ;
      if (check(logname, i, tmpactive) < 0)
      {
        genset_delete(services, i) ;
        dir_close(dir) ;
        set_scan_timeout(-i) ;
        return ;
      }
    }
  }
  dir_close(dir) ;
  if (errno)
  {
    strerr_warnwu1sys("readdir .") ;
    set_scan_timeout(5) ;
    return ;
  }
  memcpy(active, tmpactive, bitarray_div8(max)) ;

  {
    uint32_t n = genset_n(services) - avltreen_len(by_devino) ;
    if (n) genset_iter(services, &remove_deadinactive_iter, &n) ;
  }
  *what &= ~16 ;
}


 /*
    On-timeout action: starter.
    This cannot be user-triggered. It runs when a service needs to (re)start.
 */

static int start_iter (void *data, void *aux)
{
  service *sv = data ;
  uint32_t i = sv - SERVICE(0) ;
  char const *const cargv[3] = { "s6-supervise", NAME(i), 0 } ;
  cspawn_fileaction fa[2] =
  {
    [0] = { .type = CSPAWN_FA_MOVE },
    [1] = { .type = CSPAWN_FA_MOVE }
  } ;
  size_t j = 0 ;

  if (!bitarray_peek(active, i)
   || sv->pid
   || tain_future(&sv->start)) return 1 ;
  if (sv->peer < max)
  {
    fa[j].x.fd2[0] = !is_logger(i) ;
    fa[j].x.fd2[1] = sv->p ;
    j++ ;
  }
  if (consoleholder && i == special)
  {
    fa[j].x.fd2[0] = 2 ;
    fa[j].x.fd2[1] = consoleholder ;
    j++ ;
  }
  
  sv->pid = cspawn(S6_BINPREFIX "s6-supervise", cargv, (char const *const *)environ, CSPAWN_FLAGS_SELFPIPE_FINISH, fa, j) ;
  if (!sv->pid)
  {
    strerr_warnwu2sys("spawn s6-supervise for ", NAME(i)) ;
    tain_addsec_g(&start_deadline, 10) ;
    return 0 ;
  }
  avltreen_insert(by_pid, i) ;
  tain_addsec_g(&sv->start, 1) ;
  (void)aux ;
  return 1 ;
}

static inline void start (void)
{
  start_deadline = tain_infinite ;
  genset_iter(services, &start_iter, 0) ;
}


 /* Main. */

static inline int control_init (void)
{
  mode_t m = umask(0) ;
  int fdctl, fdlck, r ;
  if (mkdir(S6_SVSCAN_CTLDIR, 0700) < 0)
  {
    struct stat st ;
    if (errno != EEXIST)
      strerr_diefu1sys(111, "mkdir " S6_SVSCAN_CTLDIR) ;
    if (stat(S6_SVSCAN_CTLDIR, &st) < 0)
      strerr_diefu1sys(111, "stat " S6_SVSCAN_CTLDIR) ;
    if (!S_ISDIR(st.st_mode))
      strerr_dief1x(100, S6_SVSCAN_CTLDIR " exists and is not a directory") ;
  }

  fdlck = open3(LCK, O_WRONLY | O_NONBLOCK | O_CREAT | O_CLOEXEC, 0600) ;
  if (fdlck < 0) strerr_diefu1sys(111, "open " LCK) ;
  r = fd_lock(fdlck, 1, 1) ;
  if (r < 0) strerr_diefu1sys(111, "lock " LCK) ;
  if (!r) strerr_dief1x(100, "another instance of s6-svscan is already running on the same directory") ;
 /* fdlck leaks but it's coe */

  if (mkfifo(CTL, 0600) < 0)
  {
    struct stat st ;
    if (errno != EEXIST)
      strerr_diefu1sys(111, "mkfifo " CTL) ;
    if (stat(CTL, &st) < 0)
      strerr_diefu1sys(111, "stat " CTL) ;
    if (!S_ISFIFO(st.st_mode))
      strerr_dief1x(100, CTL " is not a FIFO") ;
  }
  fdctl = openc_read(CTL) ;
  if (fdctl < 0)
    strerr_diefu1sys(111, "open " CTL " for reading") ;
  r = openc_write(CTL) ;
  if (r < 0)
    strerr_diefu1sys(111, "open " CTL " for writing") ;
 /* r leaks but it's coe */

  umask(m) ;
  return fdctl ;
}

int main (int argc, char const *const *argv)
{
  iopause_fd x[2] = { { .fd = -1, .events = IOPAUSE_READ }, { .fd = -1, .events = IOPAUSE_READ } } ;
  PROG = "s6-svscan" ;

  {
    subgetopt l = SUBGETOPT_ZERO ;
    unsigned int notif = 0 ;
    unsigned int t = 0 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "c:C:L:t:d:X:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'c' : if (!uint320_scan(l.arg, &max)) dieusage() ; max <<= 1 ; break ;
        case 'C' : if (!uint320_scan(l.arg, &max)) dieusage() ; break ;
        case 'L' : if (!uint320_scan(l.arg, &namemax)) dieusage() ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        case 'd' : if (!uint0_scan(l.arg, &notif)) dieusage() ; break ;
        case 'X' : if (!uint0_scan(l.arg, &consoleholder)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&scantto, t) ;
    if (max < 4) max = 4 ;
    if (max > 160000) max = 160000 ;
    special = max ;
    if (namemax < 11) namemax = 11 ;
    if (namemax > 1019) namemax = 1019 ;

    if (notif)
    {
      if (notif < 3) strerr_dief1x(100, "notification fd must be 3 or more") ;
      if (fcntl(notif, F_GETFD) == -1) strerr_dief1sys(100, "invalid notification fd") ;
    }
    if (consoleholder)
    {
      if (consoleholder < 3) strerr_dief1x(100, "console holder fd must be 3 or more") ;
      if (fcntl(consoleholder, F_GETFD) < 0) strerr_dief1sys(100, "invalid console holder fd") ;
      if (coe(consoleholder) == -1) strerr_diefu1sys(111, "coe console holder") ;
    }
    if (!fd_sanitize()) strerr_diefu1x(100, "sanitize standard fds") ;

    if (argc && (chdir(argv[0]) == -1)) strerr_diefu1sys(111, "chdir") ;
    x[1].fd = control_init() ;
    x[0].fd = selfpipe_init() ;
    if (x[0].fd < 0) strerr_diefu1sys(111, "selfpipe_init") ;

    if (!sig_altignore(SIGPIPE)) strerr_diefu1sys(111, "ignore SIGPIPE") ;
    {
      sigset_t set ;
      sigemptyset(&set) ;
      sigaddset(&set, SIGCHLD) ;
      sigaddset(&set, SIGALRM) ;
      sigaddset(&set, SIGABRT) ;
      sigaddset(&set, SIGHUP) ;
      sigaddset(&set, SIGINT) ;
      sigaddset(&set, SIGTERM) ;
      sigaddset(&set, SIGQUIT) ;
      sigaddset(&set, SIGUSR1) ;
      sigaddset(&set, SIGUSR2) ;
#ifdef SIGPWR
      sigaddset(&set, SIGPWR) ;
#endif
#ifdef SIGWINCH
      sigaddset(&set, SIGWINCH) ;
#endif
      if (!selfpipe_trapset(&set)) strerr_diefu1sys(111, "trap signals") ;
    }
    if (notif)
    {
      write(notif, "\n", 1) ;
      close(notif) ;
    }
  }

  {
    unsigned int what = 0 ;
    service services_storage[max] ;
    uint32_t services_freelist[max] ;
    avlnode bydevino_storage[max] ;
    uint32_t bydevino_freelist[max] ;
    avlnode bypid_storage[max] ;
    uint32_t bypid_freelist[max] ;
    genset services_info ;
    avltreen bydevino_info ;
    avltreen bypid_info ;
    char name_storage[max * (namemax + 5)] ;
    char active_storage[bitarray_div8(max)] ;

    GENSET_init(&services_info, service, services_storage, services_freelist, max) ;
    avltreen_init(&bydevino_info, bydevino_storage, bydevino_freelist, max, &bydevino_dtok, &bydevino_cmp, &services_info) ;
    avltreen_init(&bypid_info, bypid_storage, bypid_freelist, max, &bypid_dtok, &bypid_cmp, &services_info) ;
    services = &services_info ;
    by_devino = &bydevino_info ;
    by_pid = &bypid_info ;
    names = name_storage ;
    active = active_storage ;

    tain_now_set_stopwatch_g() ;

    /* From now on, we must not die.
       Temporize on recoverable errors, and panic on serious ones. */

    while (flags.cont)
    {
      int r ;
      tain deadline = scan_deadline ;
      tain_earliest1(&deadline, &start_deadline) ;
      killthem(&what) ;
      reap(&what) ;
      r = iopause_g(x, 2, &deadline) ;
      if (r < 0) panic("iopause") ;
      else if (!r)
      {
        if (!tain_future(&scan_deadline)) scan(&what) ;
        if (!tain_future(&start_deadline)) start() ;
      }
      else
      {
        if ((x[0].revents | x[1].revents) & IOPAUSE_EXCEPT)
        {
          errno = EIO ;
          panic("check internal pipes") ;
        }
        if (x[0].revents & IOPAUSE_READ) handle_signals(&what) ;
        if (x[1].revents & IOPAUSE_READ) handle_control(x[1].fd, &what) ;
      }
    }

    /* Finish phase. */

    close_pipes() ;
    restore_console() ;
    if (flags.waitall) waitthem() ;
    selfpipe_finish() ;
  }
  {
    char const *eargv[2] = { FINISH_PROG, 0 } ;
    execv(eargv[0], (char **)eargv) ;
    if (errno != ENOENT) panicnosp("exec finish script " FINISH_PROG) ;
  }
  return 0 ;
}
