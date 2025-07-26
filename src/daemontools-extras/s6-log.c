/* ISC license. */

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

#include <skalibs/posixplz.h>
#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>
#include <skalibs/error.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr.h>
#include <skalibs/bufalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/tai.h>
#include <skalibs/djbtime.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/direntry.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/siovec.h>
#include <skalibs/cspawn.h>

#include <s6/config.h>

#ifdef S6_USE_EXECLINE
#include <execline/config.h>
#endif

#define USAGE "s6-log [ -d notif ] [ -q | -v ] [ -b ] [ -p ] [ -l linelimit ] [ -t lastlinetimeout ] [ -- ] logging_script"
#define dieusage() strerr_dieusage(100, USAGE)
#define dienomem() strerr_diefu1sys(111, "stralloc_catb")

#define LINELIMIT_MIN 48

static mode_t mask ;
static int flagprotect = 0 ;
static int flagexiting = 0 ;
static unsigned int verbosity = 1 ;
static tain lastlinetto = TAIN_INFINITE_RELATIVE ;
static tain exit_deadline = TAIN_INFINITE ;

static stralloc indata = STRALLOC_ZERO ;

 /* Data types */

typedef int qcmp_func (void const *, void const *) ;
typedef qcmp_func *qcmp_func_ref ;

typedef enum rotstate_e rotstate_t, *rotstate_t_ref ;
enum rotstate_e
{
  ROTSTATE_WRITABLE,
  ROTSTATE_FLUSHING,
  ROTSTATE_START,
  ROTSTATE_RENAME,
  ROTSTATE_NEWCURRENT,
  ROTSTATE_CHMODPREVIOUS,
  ROTSTATE_FINISHPREVIOUS,
  ROTSTATE_RUNPROCESSOR,
  ROTSTATE_WAITPROCESSOR,
  ROTSTATE_SYNCPROCESSED,
  ROTSTATE_SYNCNEWSTATE,
  ROTSTATE_UNLINKPREVIOUS,
  ROTSTATE_RENAMESTATE,
  ROTSTATE_FINISHPROCESSED,
  ROTSTATE_ENDFCHMOD,
  ROTSTATE_END
} ;

typedef enum seltype_e seltype_t, *seltype_t_ref ;
enum seltype_e
{
  SELTYPE_DEFAULT,
  SELTYPE_PLUS,
  SELTYPE_MINUS,
  SELTYPE_PHAIL
} ;

typedef struct sel_s sel_t, *sel_t_ref ;
struct sel_s
{
  seltype_t type ;
  regex_t re ;
} ;

#define SEL_ZERO { .type = SELTYPE_PHAIL }

typedef enum acttype_e acttype_t, *acttype_t_ref ;
enum acttype_e
{
  ACTTYPE_NOTHING,
  ACTTYPE_FD1,
  ACTTYPE_FD2,
  ACTTYPE_STATUS,
  ACTTYPE_DIR,
  ACTTYPE_PHAIL
} ;

typedef struct as_status_s as_status_t, *as_status_t_ref ;
struct as_status_s
{
  char const *file ;
  size_t filelen ;
} ;

typedef union actstuff_u actstuff_t, *actstuff_t_ref ;
union actstuff_u
{
  size_t fd2_size ;
  as_status_t status ;
  unsigned int ld ;
} ;

typedef struct act_s act_t, *act_t_ref ;
struct act_s
{
  acttype_t type ;
  actstuff_t data ;
  unsigned int flags ;
  char const *prefix ;
  size_t prefixlen ;
} ;

typedef struct scriptelem_s scriptelem_t, *scriptelem_t_ref ;
struct scriptelem_s
{
  sel_t const *sels ;
  unsigned int sellen ;
  act_t const *acts ;
  unsigned int actlen ;
} ;

typedef void inputproc_func (scriptelem_t const *, unsigned int, size_t, unsigned int) ;
typedef inputproc_func *inputproc_func_ref ;

typedef struct logdir_s logdir_t, *logdir_t_ref ;
struct logdir_s
{
  bufalloc out ;
  unsigned int xindex ;
  tain retrytto ;
  tain rotdeadline ;
  tain retrydeadline ;
  uint64_t maxdirsize ;
  uint32_t rotsecs ;
  uint32_t b ;
  uint32_t n ;
  uint32_t s ;
  uint32_t tolerance ;
  pid_t pid ;
  char const *dir ;
  char const *processor ;
  unsigned int flags ;
  int fd ;
  int fdlock ;
  rotstate_t rstate ;
} ;

#define LOGDIR_ZERO { \
  .out = BUFALLOC_ZERO, \
  .xindex = 0, \
  .retrytto = TAIN_ZERO, \
  .deadline = TAIN_INFINITE, \
  .maxdirsize = 0, \
  .rotsecs = 0, \
  .b = 0, \
  .n = 0, \
  .s = 0, \
  .tolerance = 0, \
  .pid = 0, \
  .dir = 0, \
  .processor = 0, \
  .fd = -1, \
  .fdlock = -1, \
  .rstate = ROTSTATE_WRITABLE \
}

struct filedesc_s
{
  off_t size ;
  char name[28] ;
} ;


 /* Logdirs */

static logdir_t *logdirs ;
static unsigned int llen = 0 ;

static int filedesc_cmp (struct filedesc_s const *a, struct filedesc_s const *b)
{
  return memcmp(a->name+1, b->name+1, 26) ;
}

static int name_is_relevant (char const *name)
{
  tain dummy ;
  if (strlen(name) != 27) return 0 ;
  if (!timestamp_scan(name, &dummy)) return 0 ;
  if (name[25] != '.') return 0 ;
  if ((name[26] != 's') && (name[26] != 'u')) return 0 ;
  return 1 ;
}

static inline int logdir_trim (logdir_t *ldp)
{
  unsigned int n = 0 ;
  DIR *dir = opendir(ldp->dir) ;
  if (!dir) return -1 ;
  for (;;)
  {
    direntry *d ;
    errno = 0 ;
    d = readdir(dir) ;
    if (!d) break ;
    if (name_is_relevant(d->d_name)) n++ ;
  }
  if (errno)
  {
    dir_close(dir) ;
    return -1 ;
  }
  if (!n)
  {
    dir_close(dir) ;
    return 0 ;
  }

  rewinddir(dir) ;

  {
    uint64_t totalsize = 0 ;
    size_t dirlen = strlen(ldp->dir) ;
    unsigned int i = 0 ;
    struct filedesc_s archive[n] ;
    char fullname[dirlen + 29] ;
    memcpy(fullname, ldp->dir, dirlen) ;
    fullname[dirlen] = '/' ;
    for (;;)
    {
      struct stat st ;
      direntry *d ;
      errno = 0 ;
      d = readdir(dir) ;
      if (!d) break ;
      if (!name_is_relevant(d->d_name)) continue ;
      if (i >= n) { errno = EBUSY ; break ; }
      memcpy(fullname + dirlen + 1, d->d_name, 28) ;
      if (stat(fullname, &st) < 0)
      {
        if (verbosity) strerr_warnwu2sys("stat ", fullname) ;
        continue ;
      }
      memcpy(archive[i].name, d->d_name, 28) ;
      archive[i].size = st.st_size ;
      totalsize += st.st_size ;
      i++ ;
    }
    if (errno)
    {
      dir_close(dir) ;
      return -1 ;
    }
    dir_close(dir) ;
    if ((i <= ldp->n) && (!ldp->maxdirsize || (totalsize <= ldp->maxdirsize)))
      return 0 ;
    qsort(archive, i, sizeof(struct filedesc_s), (qcmp_func_ref)&filedesc_cmp) ;
    n = 0 ;
    while ((i > ldp->n + n) || (ldp->maxdirsize && (totalsize > ldp->maxdirsize)))
    {
      memcpy(fullname + dirlen + 1, archive[n].name, 28) ;
      if (unlink(fullname) < 0)
      {
        if (errno == ENOENT) totalsize -= archive[n].size ;
        if (verbosity) strerr_warnwu2sys("unlink ", fullname) ;
      }
      else totalsize -= archive[n].size ;
      n++ ;
    }
  }
  return n ;
}

static void logdir_set_writable (logdir_t *ldp)
{
  ldp->rstate = ROTSTATE_WRITABLE ;
  tain_add_g(&ldp->retrydeadline, &tain_infinite_relative) ;
}

static int finish (logdir_t *ldp, char const *name, char suffix)
{
  struct stat st ;
  size_t dirlen = strlen(ldp->dir) ;
  size_t namelen = strlen(name) ;
  char x[dirlen + namelen + 2] ;
  memcpy(x, ldp->dir, dirlen) ;
  x[dirlen] = '/' ;
  memcpy(x + dirlen + 1, name, namelen + 1) ;
  if (stat(x, &st) < 0) return errno == ENOENT ? 0 : -1 ;
  if (st.st_nlink == 1 && st.st_size)
  {
    char y[dirlen + 29] ;
    memcpy(y, ldp->dir, dirlen) ;
    y[dirlen] = '/' ;
    timestamp_g(y + dirlen + 1) ;
    y[dirlen + 26] = '.' ;
    y[dirlen + 27] = suffix ;
    y[dirlen + 28] = 0 ;
    if (link(x, y) < 0) return -1 ;
  }
  if (unlink(x) < 0) return -1 ;
  return logdir_trim(ldp) ;
}

static int rotator (logdir_t *ldp)
{
  size_t dirlen = strlen(ldp->dir) ;
  switch (ldp->rstate)
  {
    case ROTSTATE_START :
      if (fd_sync(ldp->fd) < 0)
      {
        if (verbosity) strerr_warnwu3sys("fd_sync ", ldp->dir, "/current") ;
        goto fail ;
      }
      tain_now_g() ;
      ldp->rstate = ROTSTATE_RENAME ;
    case ROTSTATE_RENAME :
    {
      char current[dirlen + 9] ;
      char previous[dirlen + 10] ;
      memcpy(current, ldp->dir, dirlen) ;
      memcpy(current + dirlen, "/current", 9) ;
      memcpy(previous, ldp->dir, dirlen) ;
      memcpy(previous + dirlen, "/previous", 10) ;
      if (rename(current, previous) < 0)
      {
        if (verbosity) strerr_warnwu4sys("rename ", current, " to ", previous) ;
        goto fail ;
      }
      ldp->rstate = ROTSTATE_NEWCURRENT ;
    }
    case ROTSTATE_NEWCURRENT :
    {
      int fd ;
      char x[dirlen + 9] ;
      memcpy(x, ldp->dir, dirlen) ;
      memcpy(x + dirlen, "/current", 9) ;
      fd = openc_append(x) ;
      if (fd < 0)
      {
        if (verbosity) strerr_warnwu2sys("open_append ", x) ;
        goto fail ;
      }
      fd_close(ldp->fd) ;
      ldp->fd = fd ;
      ldp->b = 0 ;
      ldp->rstate = ROTSTATE_CHMODPREVIOUS ;
    }
    case ROTSTATE_CHMODPREVIOUS :
    {
      char x[dirlen + 10] ;
      memcpy(x, ldp->dir, dirlen) ;
      memcpy(x + dirlen, "/previous", 10) ;
      if (chmod(x, mask | S_IXUSR) < 0)
      {
        if (verbosity) strerr_warnwu2sys("chmod ", x) ;
        goto fail ;
      }
      if (ldp->processor) goto runprocessor ;
      ldp->rstate = ROTSTATE_FINISHPREVIOUS ;
    }
    case ROTSTATE_FINISHPREVIOUS :
      if (finish(ldp, "previous", 's') < 0)
      {
        if (verbosity) strerr_warnwu2sys("finish previous .s to logdir ", ldp->dir) ;
        goto fail ;
      }
      logdir_set_writable(ldp) ;
      break ;
   runprocessor:
      ldp->rstate = ROTSTATE_RUNPROCESSOR ;
    case ROTSTATE_RUNPROCESSOR :
    {
#ifdef S6_USE_EXECLINE
      char const *cargv[4] = { ldp->flags & 4 ? "/bin/sh" : EXECLINE_EXTBINPREFIX "execlineb", ldp->flags & 4 ? "-c" : "-Pc", ldp->processor, 0 } ;
#else
      char const *cargv[4] = { "/bin/sh", "-c", ldp->processor, 0 } ;
#endif
      cspawn_fileaction fa[5] =
      {
        [0] = { .type = CSPAWN_FA_CHDIR, .x = { .path = ldp->dir } }, 
        [1] = { .type = CSPAWN_FA_OPEN, .x = { .openinfo = { .fd = 0, .file = "previous", .oflag = O_RDONLY, .mode = 0644 } } }, 
        [2] = { .type = CSPAWN_FA_OPEN, .x = { .openinfo = { .fd = 1, .file = "processed", .oflag = O_WRONLY | O_CREAT | O_TRUNC, .mode = 0666 } } }, 
        [3] = { .type = CSPAWN_FA_OPEN, .x = { .openinfo = { .fd = 4, .file = "state", .oflag = O_RDONLY, .mode = 0644 } } }, 
        [4] = { .type = CSPAWN_FA_OPEN, .x = { .openinfo = { .fd = 5, .file = "newstate", .oflag = O_WRONLY | O_CREAT | O_TRUNC, .mode = 0666 } } }
      } ;
      pid_t pid = cspawn(cargv[0], cargv, (char const *const *)environ, CSPAWN_FLAGS_SELFPIPE_FINISH, fa, 5) ;
      if (!pid)
      {
        if (verbosity) strerr_warnwu2sys("spawn processor for logdir ", ldp->dir) ;
        goto fail ;
      }
      ldp->pid = pid ;
      tain_add_g(&ldp->retrydeadline, &tain_infinite_relative) ;
      ldp->rstate = ROTSTATE_WAITPROCESSOR ;
    }
    case ROTSTATE_WAITPROCESSOR :
      return (errno = EAGAIN, 0) ;
    case ROTSTATE_SYNCPROCESSED :
    {
      int fd ;
      char x[dirlen + 11] ;
      memcpy(x, ldp->dir, dirlen) ;
      memcpy(x + dirlen, "/processed", 11) ;
      fd = open_append(x) ;
      if (fd < 0)
      {
        if (verbosity) strerr_warnwu2sys("open_append ", x) ;
        goto fail ;
      }
      if (fd_sync(fd) < 0)
      {
        fd_close(fd) ;
        if (verbosity) strerr_warnwu2sys("fd_sync ", x) ;
        goto fail ;
      }
      tain_now_g() ;
      if (fd_chmod(fd, mask | S_IXUSR) < 0)
      {
        fd_close(fd) ;
        if (verbosity) strerr_warnwu2sys("fd_chmod ", x) ;
        goto fail ;
      }
      fd_close(fd) ;
      ldp->rstate = ROTSTATE_SYNCNEWSTATE ;
    }
    case ROTSTATE_SYNCNEWSTATE :
    {
      int fd ;
      char x[dirlen + 10] ;
      memcpy(x, ldp->dir, dirlen) ;
      memcpy(x + dirlen, "/newstate", 10) ;
      fd = open_append(x) ;
      if (fd < 0)
      {
        if (verbosity) strerr_warnwu2sys("open_append ", x) ;
        goto fail ;
      }
      if (fd_sync(fd) < 0)
      {
        if (verbosity) strerr_warnwu2sys("fd_sync ", x) ;
        goto fail ;
      }
      tain_now_g() ;
      fd_close(fd) ;
      ldp->rstate = ROTSTATE_UNLINKPREVIOUS ;
    }
    case ROTSTATE_UNLINKPREVIOUS :
    {
      char x[dirlen + 10] ;
      memcpy(x, ldp->dir, dirlen) ;
      memcpy(x + dirlen, "/previous", 10) ;
      if ((unlink(x) < 0) && (errno != ENOENT))
      {
        if (verbosity) strerr_warnwu2sys("open_append ", x) ;
        goto fail ;
      }
      ldp->rstate = ROTSTATE_RENAMESTATE ;
    }
    case ROTSTATE_RENAMESTATE :
    {
      char newstate[dirlen + 10] ;
      char state[dirlen + 7] ;
      memcpy(newstate, ldp->dir, dirlen) ;
      memcpy(state, ldp->dir, dirlen) ;
      memcpy(newstate + dirlen, "/newstate", 10) ;
      memcpy(state + dirlen, "/state", 7) ;
      if (rename(newstate, state) < 0)
      {
        if (verbosity) strerr_warnwu4sys("rename ", newstate, " to ", state) ;
        goto fail ;
      }
      ldp->rstate = ROTSTATE_FINISHPROCESSED ;
    }
    case ROTSTATE_FINISHPROCESSED :
      if (finish(ldp, "processed", 's') < 0)
      {
        if (verbosity) strerr_warnwu2sys("finish processed .s to logdir ", ldp->dir) ;
        goto fail ;
      }
      logdir_set_writable(ldp) ;
      break ;
    default : strerr_dief1x(101, "inconsistent state in rotator()") ;
  }
  return 1 ;
 fail:
   tain_add_g(&ldp->retrydeadline, &ldp->retrytto) ;
   return 0 ;
}

static void logdir_flush (logdir_t *ldp)
{
  if (bufalloc_flush(&ldp->out)) logdir_set_writable(ldp) ;
}

static void logdir_rotate (logdir_t *ldp)
{
  enum rotstate_e oldstate = ldp->rstate ;
  if (ldp->rstate == ROTSTATE_FLUSHING || ldp->rstate == ROTSTATE_WRITABLE) ldp->rstate = ROTSTATE_START ;
  if (!rotator(ldp)) return ;
  if (oldstate == ROTSTATE_FLUSHING)
  {
    tain_add_g(&ldp->retrydeadline, &ldp->retrytto) ;
    ldp->rstate = ROTSTATE_FLUSHING ;
  }
}

static void logdir_retry (logdir_t *ldp)
{
  if (ldp->rstate == ROTSTATE_FLUSHING) logdir_flush(ldp) ;
  else rotator(ldp) ;
}

static ssize_t logdir_write (int i, char const *s, size_t len)
{
  logdir_t *ldp = logdirs + i ;
  ssize_t r ;
  size_t n = len ;
  {
    size_t m = byte_rchr(s, n, '\n') ;
    if (m < n) n = m+1 ;
  }
  r = fd_write(ldp->fd, s, n) ;
  if (r <= 0)
  {
    ldp->rstate = ROTSTATE_FLUSHING ;
    tain_add_g(&ldp->retrydeadline, &ldp->retrytto) ;
    return -1 ;
  }
  ldp->b += r ;
  if (ldp->b + ldp->tolerance >= ldp->s && s[r-1] == '\n')
  {
    ldp->rstate = ROTSTATE_START ;
    return rotator(ldp) ? r : -1 ;
  }
  else return r ;
}

static inline void logdir_init (unsigned int index, uint32_t s, uint32_t n, uint32_t tolerance, uint64_t maxdirsize, uint32_t rotsecs, tain const *retrytto, char const *processor, char const *name, unsigned int flags)
{
  logdir_t *ldp = logdirs + index ;
  struct stat st ;
  size_t dirlen = strlen(name) ;
  int r ;
  char x[dirlen + 11] ;
  ldp->s = s ;
  ldp->n = n ;
  ldp->pid = 0 ;
  ldp->tolerance = tolerance ;
  ldp->maxdirsize = maxdirsize ;
  ldp->rotsecs = rotsecs ;
  ldp->retrytto = *retrytto ;
  ldp->processor = processor ;
  ldp->flags = flags ;
  ldp->dir = name ;
  ldp->fd = -1 ;
  r = mkdir(ldp->dir, S_IRWXU | S_ISGID) ;
  if (r < 0 && errno != EEXIST) strerr_diefu2sys(111, "mkdir ", name) ;
  memcpy(x, name, dirlen) ;
  memcpy(x + dirlen, "/lock", 6) ;
  ldp->fdlock = openc_create(x) ;
  if (ldp->fdlock < 0) strerr_diefu2sys(111, "open ", x) ;
  r = fd_lock(ldp->fdlock, 1, 1) ;
  if (!r) errno = EBUSY ;
  if (r < 1) strerr_diefu2sys(111, "lock ", x) ;
  memcpy(x + dirlen + 1, "current", 8) ;
  if (stat(x, &st) < 0)
  {
    if (errno != ENOENT) strerr_diefu2sys(111, "stat ", x) ;
  }
  else if (st.st_mode & S_IXUSR) goto opencurrent ;
  memcpy(x + dirlen + 1, "state", 6) ;
  unlink_void(x) ;
  memcpy(x + dirlen + 1, "newstate", 9) ;
  unlink_void(x) ;
  {
    int flagprocessed = 0 ;
    memcpy(x + dirlen + 1, "processed", 10) ;
    if (stat(x, &st) < 0)
    {
      if (errno != ENOENT) strerr_diefu2sys(111, "stat ", x) ;
    }
    else if (st.st_mode & S_IXUSR) flagprocessed = 1 ;
    if (flagprocessed)
    {
      memcpy(x + dirlen + 1, "previous", 9) ;
      unlink_void(x) ;
      if (finish(ldp, "processed", 's') < 0)
        strerr_diefu2sys(111, "finish processed .s for logdir ", ldp->dir) ;
    }
    else
    {
      unlink_void(x) ;
      if (finish(ldp, "previous", 'u') < 0)
        strerr_diefu2sys(111, "finish previous .u for logdir ", ldp->dir) ;
    }
  }
  if (finish(ldp, "current", 'u') < 0)
    strerr_diefu2sys(111, "finish current .u for logdir ", ldp->dir) ;
  memcpy(x + dirlen + 1, "state", 6) ;
  r = open_trunc(x) ;
  if (r == -1) strerr_diefu2sys(111, "open_trunc ", x) ;
  fd_close(r) ;
  st.st_size = 0 ;
  memcpy(x + dirlen + 1, "current", 8) ;
 opencurrent:
  ldp->fd = openc_append(x) ;
  if (ldp->fd < 0) strerr_diefu2sys(111, "open_append ", x) ;
  if (ndelay_off(ldp->fd) == -1)
    strerr_diefu2sys(111, "ndelay_off ", x) ;
  if (fd_chmod(ldp->fd, mask) == -1)
    strerr_diefu2sys(111, "fd_chmod ", x) ;
  ldp->b = st.st_size ;
  if (rotsecs) tain_addsec_g(&ldp->rotdeadline, rotsecs) ;
  else tain_add_g(&ldp->rotdeadline, &tain_infinite_relative) ;
  logdir_set_writable(ldp) ;
  bufalloc_init(&ldp->out, &logdir_write, index) ;
}

static inline int logdir_finalize (logdir_t *ldp)
{
  switch (ldp->rstate)
  {
    case ROTSTATE_WRITABLE :
    {
      if (fd_sync(ldp->fd) < 0)
      {
        if (verbosity) strerr_warnwu3sys("fd_sync ", ldp->dir, "/current") ;
        goto fail ;
      }
      tain_now_g() ;
      ldp->rstate = ROTSTATE_ENDFCHMOD ;
    }
    case ROTSTATE_ENDFCHMOD :
    {
      if (fd_chmod(ldp->fd, mask | S_IXUSR) < 0)
      {
        if (verbosity) strerr_warnwu3sys("fd_chmod ", ldp->dir, "/current") ;
        goto fail ;
      }
      ldp->rstate = ROTSTATE_END ;
      break ;
    }
    default : strerr_dief1x(101, "inconsistent state in logdir_finalize()") ;
  }
  return 1 ;
 fail:
  tain_add_g(&ldp->retrydeadline, &ldp->retrytto) ;
  return 0 ;
}

static inline void finalize (void)
{
  unsigned int n = llen ;
  for (;;)
  {
    unsigned int i = 0 ;
    tain deadline ;
    tain_addsec_g(&deadline, 2) ;
    for (; i < llen ; i++)
      if (logdirs[i].rstate != ROTSTATE_END)
      {
        if (logdir_finalize(logdirs + i)) n-- ;
        else if (tain_less(&logdirs[i].retrydeadline, &deadline))
          deadline = logdirs[i].retrydeadline ;
      }
    if (!n) break ;
    {
      iopause_fd x ;
      iopause_g(&x, 0, &deadline) ;
    }
  }
}


 /* Script */
 
static inline void script_firstpass (char const *const *argv, unsigned int *sellen, unsigned int *actlen, unsigned int *scriptlen, unsigned int *gflags)
{
  unsigned int se = 0, ac = 0, sc = 0, gf = *gflags ;
  int flagacted = 0 ;
  for (; *argv ; argv++)
  {
    switch ((*argv)[0])
    {
      case 'f' :
        if ((*argv)[1]) goto fail ;
      case '+' :
      case '-' :
        if (flagacted)
        {
          sc++ ;
          flagacted = 0 ;
        }
        se++ ;
      case 'n' :
      case 's' :
      case 'S' :
      case 'l' :
      case 'r' :
      case 'R' :
      case 'E' :
      case '^' :
      case 'p' :
#ifdef S6_USE_EXECLINE
      case '!' :
#endif
      case '?' :
        break ;
      case 't' :
        if ((*argv)[1]) goto fail ;
        gf |= 1 ;
        break ;
      case 'T' :
        if ((*argv)[1]) goto fail ;
        gf |= 2 ;
        break ;
      case '1' :
      case '2' :
        if ((*argv)[1]) goto fail ;
        flagacted = 1 ;
        ac++ ;
        break ;
      case '.' :
      case '/' :
        llen++ ;
        flagacted = 1 ;
        ac++ ;
        break ;
      case '=' :
        if (!(*argv)[1]) goto fail ;
        flagacted = 1 ;
        ac++ ;
        break ;
      default : strerr_dief2x(100, "unrecognized directive: ", *argv) ;
    }
  }
  if (flagacted) sc++ ;
  else if (sc)
  {
    if (verbosity)
      strerr_warnw1x("ignoring extraneous non-action directives") ;
  }
  else strerr_dief1x(100, "no action directive specified") ;
  *sellen = se ;
  *actlen = ac ;
  *scriptlen = sc ;
  *gflags = gf ;
  return ;
 fail :
  strerr_dief2x(100, "syntax error at directive: ", *argv) ;
}

static inline void script_secondpass (char const *const *argv, scriptelem_t *script, sel_t *selections, act_t *actions)
{
  tain retrytto ;
  unsigned int fd2_size = 200 ;
  unsigned int status_size = 1001 ;
  uint32_t rotsecs = 0 ;
  uint32_t s = 99999 ;
  uint32_t n = 10 ;
  uint32_t tolerance = 2000 ;
  uint64_t maxdirsize = 0 ;
  char const *processor = 0 ;
  char const *prefix = 0 ;
  size_t prefixlen = 0 ;
  unsigned int sel = 0, act = 0, lidx = 0, flags = 0 ;
  int flagacted = 0 ;
  tain_uint(&retrytto, 2) ;
  
  for (; *argv ; argv++)
  {
    switch (**argv)
    {
      case 'f' :
      case '+' :
      case '-' :
      {
        sel_t selitem = { .type = (*argv)[0] != 'f' ? (*argv)[0] == '+' ? SELTYPE_PLUS : SELTYPE_MINUS : SELTYPE_DEFAULT } ;
        if ((*argv)[0] != 'f')
        {
          int r = skalibs_regcomp(&selitem.re, *argv + 1, REG_EXTENDED | REG_NOSUB | REG_NEWLINE) ;
          if (r == REG_ESPACE)
          {
            errno = ENOMEM ;
            strerr_diefu1sys(111, "initialize script") ;
          }
          if (r) goto fail ;
        }
        if (flagacted)
        {
          flagacted = 0 ;
          script->sels = selections ;
          script->sellen = sel ;
          script->acts = actions ;
          script->actlen = act ;
          selections += sel ; sel = 0 ;
          actions += act ; act = 0 ;
          script++ ;
        }
        selections[sel++] = selitem ;
        break ;
      }
      case 'n' :
        if (!uint320_scan(*argv + 1, &n)) goto fail ;
        break ;
      case 's' :
        if (!uint320_scan(*argv + 1, &s)) goto fail ;
        if (s < 4096) s = 4096 ;
        if (s > 268435455) s = 268435455 ;
        break ;
      case 'S' :
        if (!uint640_scan(*argv + 1, &maxdirsize)) goto fail ;
        break ;
      case 'l' :
        if (!uint320_scan(*argv + 1, &tolerance)) goto fail ;
        if (tolerance > (s >> 1))
          strerr_dief3x(100, "directive ", *argv, " conflicts with previous s directive") ;
        break ;
      case 'r' :
      {
        uint32_t t ;
        if (!uint320_scan(*argv + 1, &t)) goto fail ;
        if (!tain_from_millisecs(&retrytto, t)) goto fail ;
        break ;
      }
      case 'R' :
        if (!uint320_scan(*argv + 1, &rotsecs)) goto fail ;
        break ;
      case 'E' :
        if (!uint0_scan(*argv + 1, &fd2_size)) goto fail ;
        break ;
      case '^' :
        if (!uint0_scan(*argv + 1, &status_size)) goto fail ;
        break ;
      case 'p' :
        if ((*argv)[1]) { prefix = *argv + 1 ; prefixlen = strlen(prefix) ; }
        else { prefix = 0 ; prefixlen = 0 ; }
        break ;
#ifdef S6_USE_EXECLINE
      case '!' :
        processor = (*argv)[1] ? *argv + 1 : 0 ;
        flags &= ~4 ;
        break ;
#endif
      case '?' :
        processor = (*argv)[1] ? *argv + 1 : 0 ;
        flags |= 4 ;
        break ;
      case 't' :
        flags |= 1 ;
        break ;
      case 'T' :
        flags |= 2 ;
        break ;
      case '1' :
      {
        act_t a = { .type = ACTTYPE_FD1, .flags = flags, .prefix = prefix, .prefixlen = prefixlen } ;
        actions[act++] = a ; flagacted = 1 ; flags = 0 ;
        break ;
      }
      case '2' :
      {
        act_t a = { .type = ACTTYPE_FD2, .flags = flags, .prefix = prefix, .prefixlen = prefixlen, .data = { .fd2_size = fd2_size } } ;
        actions[act++] = a ; flagacted = 1 ; flags = 0 ;
        break ;
      }
      case '=' :
      {
        act_t a = { .type = ACTTYPE_STATUS, .flags = flags, .prefix = prefix, .prefixlen = prefixlen, .data = { .status = { .file = *argv + 1, .filelen = status_size } } } ;
        actions[act++] = a ; flagacted = 1 ; flags = 0 ;
        break ;
      }
      case '.' : 
      case '/' :
      {
        act_t a = { .type = ACTTYPE_DIR, .flags = flags, .prefix = prefix, .prefixlen = prefixlen, .data = { .ld = lidx } } ;
        logdir_init(lidx, s, n, tolerance, maxdirsize, rotsecs, &retrytto, processor, *argv, flags) ;
        lidx++ ;
        actions[act++] = a ; flagacted = 1 ; flags = 0 ;
        break ;
      }
      default : goto fail ;
    }
  }
  if (flagacted)
  {
    script->sels = selections ;
    script->sellen = sel ;
    script->acts = actions ;
    script->actlen = act ;
  }
  return ;
 fail:
  strerr_dief2x(100, "unrecognized directive: ", *argv) ;
}

static void script_run (scriptelem_t const *script, unsigned int scriptlen, char const *s, size_t len, unsigned int gflags)
{
  int flagselected = 1, flagacted = 0 ;
  unsigned int i = 0 ;
  size_t hlen = 0 ;
  char hstamp[32] ;
  char tstamp[TIMESTAMP+1] ;
  if (gflags & 3)
  {
    tain now ;
    tain_wallclock_read(&now) ;
    if (gflags & 1)
    {
      timestamp_fmt(tstamp, &now) ;
      tstamp[TIMESTAMP] = ' ' ;
    }
    if (gflags & 2)
    {
      localtmn l ;
      localtmn_from_tain(&l, &now, 1) ;
      hlen = localtmn_fmt(hstamp, &l) ;
      hstamp[hlen++] = ' ' ;
      hstamp[hlen++] = ' ' ;
    }
  }
  
  for (; i < scriptlen ; i++)
  {
    unsigned int j = 0 ;
    for (; j < script[i].sellen ; j++)
    {
      switch (script[i].sels[j].type)
      {
        case SELTYPE_DEFAULT :
          flagselected = !flagacted ;
          break ;
        case SELTYPE_PLUS :
	  if (!flagselected && !regexec(&script[i].sels[j].re, s, 0, 0, 0)) flagselected = 1 ;
          break ;
        case SELTYPE_MINUS :
	  if (flagselected && !regexec(&script[i].sels[j].re, s, 0, 0, 0)) flagselected = 0 ;
          break ;
        default :
          strerr_dief2x(101, "internal consistency error in ", "selection type") ;
      }
    }
    if (flagselected)
    {
      flagacted = 1 ;
      for (j = 0 ; j < script[i].actlen ; j++)
      {
        act_t const *act = script[i].acts + j ;
        unsigned int m = 0 ;
        struct iovec v[6] ;
        if (act->flags & 1)
        {
          v[m].iov_base = tstamp ;
          v[m++].iov_len = TIMESTAMP+1 ;
        }
        if (act->flags & 2)
        {
          v[m].iov_base = hstamp ;
          v[m++].iov_len = hlen ;
        }
        if (act->prefix)
        {
          v[m].iov_base = (char *)act->prefix ;
          v[m++].iov_len = act->prefixlen ;
          v[m].iov_base = " " ;
          v[m++].iov_len = 1 ;
        }
        v[m].iov_base = (char *)s ;
        v[m++].iov_len = len ;
        v[m].iov_base = "\n" ;
        v[m++].iov_len = 1 ;
        switch (act->type)
        {
          case ACTTYPE_FD1 :
            if (!bufalloc_putv(bufalloc_1, v, m)) dienomem() ;
          case ACTTYPE_NOTHING :
            break ;

          case ACTTYPE_FD2 :
            buffer_puts(buffer_2, PROG) ;
            buffer_puts(buffer_2, ": alert: ") ;
            if (act->data.fd2_size && act->data.fd2_size + 3 < len)
            {
              v[m-2].iov_len = act->data.fd2_size ;
              v[m-1].iov_base = "...\n" ;
              v[m-1].iov_len = 4 ;
            }
            buffer_putv(buffer_2, v, m) ;
            buffer_flush(buffer_2) ; /* if it blocks, too bad */
            break ;

          case ACTTYPE_STATUS :
            if (act->data.status.filelen)
            {
              size_t reallen = siovec_len(v, m) ;
              if (reallen > act->data.status.filelen)
                siovec_trunc(v, m, act->data.status.filelen) ;
              else
              {
                size_t k = act->data.status.filelen - reallen + 1 ;
                char pad[k] ;
                v[m-1].iov_base = pad ;
                v[m-1].iov_len = k ;
                while (k--) pad[k] = '\n' ;
                if (!openwritevnclose_suffix(act->data.status.file, v, m, ".new") && verbosity)
                  strerr_warnwu2sys("write status file ", act->data.status.file) ;
                break ;
              }
            }
            if (!openwritevnclose_suffix(act->data.status.file, v, m, ".new") && verbosity)
              strerr_warnwu2sys("write status file ", act->data.status.file) ;
            break ;

          case ACTTYPE_DIR :
            if (!bufalloc_putv(&logdirs[act->data.ld].out, v, m)) dienomem() ;
            break ;

          default :
            strerr_dief2x(101, "internal consistency error in ", "action type") ;
        }
      }
    }
  }
  if (gflags & 3) tain_now_g() ;
}


 /* Input */

static void prepare_to_exit (void)
{
  fd_close(0) ;
  flagexiting = 1 ;
}

static inline int getchunk (buffer *b, stralloc *sa, size_t linelimit)
{
  struct iovec v[2] ;
  size_t pos ;
  int r ;
  buffer_rpeek(b, v) ;
  pos = siovec_bytein(v, 2, "\n", 2) ;
  if (linelimit && sa->len + pos > linelimit)
  {
    r = 2 ;
    pos = linelimit - sa->len ;
  }
  else
  {
    r = pos < buffer_len(b) ;
    pos += r ;
  }
  if (!stralloc_readyplus(sa, pos + (r == 2))) return -1 ;
  buffer_getnofill(b, sa->s + sa->len, pos) ; sa->len += pos ;
  if (r == 2) sa->s[sa->len++] = 0 ;
  return r ;
}

static void normal_stdin (scriptelem_t const *script, unsigned int scriptlen, size_t linelimit, unsigned int gflags)
{
  ssize_t r = sanitize_read(buffer_fill(buffer_0)) ;
  if (r < 0)
  {
    if ((errno != EPIPE) && verbosity) strerr_warnwu1sys("read from stdin") ;
    prepare_to_exit() ;
  }
  else if (r) for (;;)
  {
    r = getchunk(buffer_0, &indata, linelimit) ;
    if (r < 0) dienomem() ;
    else if (!r) break ;
    indata.s[indata.len - 1] = 0 ;
    script_run(script, scriptlen, indata.s, indata.len - 1, gflags) ;
    indata.len = 0 ;
  }
}

static void process_partial_line (scriptelem_t const *script, unsigned int scriptlen, unsigned int gflags)
{
  if (!stralloc_0(&indata)) dienomem() ;
  script_run(script, scriptlen, indata.s, indata.len - 1, gflags) ;
  indata.len = 0 ;
}

static void last_stdin (scriptelem_t const *script, unsigned int scriptlen, size_t linelimit, unsigned int gflags)
{
  for (;;)
  {
    char c ;
    switch (sanitize_read(fd_read(0, &c, 1)))
    {
      case 0 : return ;
      case -1 :
        if ((errno != EPIPE) && verbosity) strerr_warnwu1sys("read from stdin") ;
        if (indata.len) goto lastline ;
        goto end ;
      case 1 :
        if (c == '\n' || !c) goto lastline ;
        if (!stralloc_catb(&indata, &c, 1)) dienomem() ;
        if (linelimit && indata.len >= linelimit)
        {
          if (verbosity) strerr_warnw2x("input line too long, ", "stopping before the end") ;
          goto lastline ;
        }
        else break ;
    }
  }
 lastline:
  process_partial_line(script, scriptlen, gflags) ;
 end:
  prepare_to_exit() ;
}

static inputproc_func_ref handle_stdin = &normal_stdin ;


 /* Signals */

static inline void processor_died (logdir_t *ldp, int wstat)
{
  ldp->pid = 0 ;
  if (WIFSIGNALED(wstat))
  {
    if (verbosity) strerr_warnw2x("processor crashed in ", ldp->dir) ;
    tain_add_g(&ldp->retrydeadline, &ldp->retrytto) ;
    ldp->rstate = ROTSTATE_RUNPROCESSOR ;
  }
  else if (WEXITSTATUS(wstat))
  {
    if (verbosity) strerr_warnw2x("processor failed in ", ldp->dir) ;
    tain_add_g(&ldp->retrydeadline, &ldp->retrytto) ;
    ldp->rstate = ROTSTATE_RUNPROCESSOR ;
  }
  else
  {
    ldp->rstate = ROTSTATE_SYNCPROCESSED ;
    rotator(ldp) ;
  }
}

static inline int handle_signals (void)
{
  int e = 0 ;
  for (;;)
  {
    switch (selfpipe_read())
    {
      case -1 : strerr_diefu1sys(111, "selfpipe_read") ;
      case 0 : return e ;
      case SIGALRM :
        for (unsigned int i = 0 ; i < llen ; i++) logdir_rotate(logdirs + i) ;
        break ;
      case SIGTERM :
        if (flagprotect) break ;
      case SIGHUP :
        handle_stdin = &last_stdin ;
        tain_add_g(&exit_deadline, &lastlinetto) ;
        if (!indata.len) { prepare_to_exit() ; e = 1 ; }
        break ;
      case SIGCHLD :
      {
        for (;;)
        {
          int wstat ;
          unsigned int i = 0 ;
          pid_t r = wait_nohang(&wstat) ;
          if (r <= 0) break ;
          for (; i < llen ; i++) if (r == logdirs[i].pid) break ;
          if (i < llen) processor_died(logdirs + i, wstat) ;
        }
        break ;
      }
      default : strerr_dief1x(101, "internal consistency error with signal handling") ;
    }
  }
}


 /* Main */

int main (int argc, char const *const *argv)
{
  unsigned int sellen, actlen, scriptlen ;
  unsigned int linelimit = 8192 ;
  unsigned int notif = 0 ;
  unsigned int gflags = 0 ;
  int flagblock = 0 ;
  PROG = "s6-log" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    unsigned int t = 2000 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "qvbpl:d:t:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'q' : if (verbosity) verbosity-- ; break ;
        case 'v' : verbosity++ ; break ;
        case 'b' : flagblock = 1 ; break ;
        case 'p' : flagprotect = 1 ; break ;
        case 'l' : if (!uint0_scan(l.arg, &linelimit)) dieusage() ; break ;
        case 'd' :
          if (!uint0_scan(l.arg, &notif)) dieusage() ;
          if (notif < 3) strerr_dief1x(100, "notification fd must be 3 or more") ;
          if (fcntl(notif, F_GETFD) < 0) strerr_dief1sys(100, "invalid notification fd") ;
          break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&lastlinetto, t) ;
    else lastlinetto = tain_infinite_relative ;
  }
  if (!argc) dieusage() ;
  if (linelimit && linelimit < LINELIMIT_MIN) linelimit = LINELIMIT_MIN ;
  if (!fd_sanitize()) strerr_diefu1sys(111, "ensure stdin/stdout/stderr are open") ;
  if (!tain_now_set_stopwatch_g() && verbosity)
    strerr_warnwu1sys("set monotonic clock and read current time - timestamps may be wrong for a while") ;
  if (ndelay_on(0) < 0) strerr_diefu3sys(111, "set std", "in", " non-blocking") ;
  if (ndelay_on(1) < 0) strerr_diefu3sys(111, "set std", "out", " non-blocking") ;
  mask = umask(0) ;
  umask(mask) ;
  mask = ~mask & 0666 ;
  script_firstpass(argv, &sellen, &actlen, &scriptlen, &gflags) ;
  {
    iopause_fd x[3] = { { .events = IOPAUSE_READ } } ;
    sel_t selections[sellen ? sellen : 1] ;
    act_t actions[actlen] ;
    scriptelem_t script[scriptlen] ;
    logdir_t logdirblob[llen] ;
    logdirs = logdirblob ;
    script_secondpass(argv, script, selections, actions) ;
    x[0].fd = selfpipe_init() ;
    if (x[0].fd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
    if (!sig_altignore(SIGPIPE)) strerr_diefu1sys(111, "sig_ignore(SIGPIPE)") ;
    {
      sigset_t set ;
      sigemptyset(&set) ;
      sigaddset(&set, SIGTERM) ;
      sigaddset(&set, SIGHUP) ;
      sigaddset(&set, SIGALRM) ;
      sigaddset(&set, SIGCHLD) ;
      if (!selfpipe_trapset(&set))
        strerr_diefu1sys(111, "selfpipe_trapset") ;
    }
    if (notif)
    {
      fd_write(notif, "\n", 1) ;
      fd_close(notif) ;
    }

    for (;;)
    {
      tain deadline = exit_deadline ;
      int r = 0 ;
      unsigned int xindex0, xindex1 ;
      unsigned int j = 1 ;
      if (bufalloc_1->fd == 1 && bufalloc_len(bufalloc_1))
      {
        r = 1 ;
        x[j].fd = 1 ;
        x[j].events = bufalloc_len(bufalloc_1) ? IOPAUSE_WRITE : 0 ;
        xindex1 = j++ ;
      }
      else xindex1 = 0 ;

      for (unsigned int i = 0 ; i < llen ; i++)
      {
        if (bufalloc_len(&logdirs[i].out))
        {
          if (logdirs[i].rstate == ROTSTATE_WRITABLE) logdir_flush(logdirs + i) ;
          if (logdirs[i].rstate != ROTSTATE_WRITABLE) r = 1 ;
        }
        if (tain_less(&logdirs[i].retrydeadline, &deadline)) deadline = logdirs[i].retrydeadline ;
        if (tain_less(&logdirs[i].rotdeadline, &deadline)) deadline = logdirs[i].rotdeadline ;
      }

      if (!flagexiting && !(flagblock && r))
      {
        x[j].fd = 0 ;
        x[j].events = IOPAUSE_READ ;
        xindex0 = j++ ;
      }
      else xindex0 = 0 ;

      if (flagexiting && !r) break ;

      r = iopause_g(x, j, &deadline) ;
      if (r < 0) strerr_diefu1sys(111, "iopause") ;
      else if (!r)
      {
        if (!tain_future(&exit_deadline))
        {
          if (indata.len) process_partial_line(script, scriptlen, gflags) ;
          prepare_to_exit() ;
        }
        for (unsigned int i = 0 ; i < llen ; i++)
        {
          if (!tain_future(&logdirs[i].retrydeadline)) logdir_retry(logdirs + i) ;
          if (!tain_future(&logdirs[i].rotdeadline))
          {
            if (logdirs[i].rotsecs) tain_addsec_g(&logdirs[i].rotdeadline, logdirs[i].rotsecs) ;
            else tain_add_g(&logdirs[i].rotdeadline, &tain_infinite_relative) ;
            logdir_rotate(logdirs + i) ;
          }
        }
        continue ;
      }

      if (x[0].revents & IOPAUSE_READ && handle_signals()) continue ;

      if (xindex1 && x[xindex1].revents)
      {
        if (!bufalloc_flush(bufalloc_1) && !error_isagain(errno))
        {
          unsigned int i = actlen ;
          strerr_warnwu1sys("write to stdout, closing the stream - error was") ;
          fd_close(1) ;
          bufalloc_1->fd = -1 ;
          bufalloc_free(bufalloc_1) ;
          while (i--)
            if (actions[i].type == ACTTYPE_FD1)
              actions[i].type = ACTTYPE_NOTHING ;
        }
      }

      if (xindex0 && x[xindex0].revents)
      {
        if (x[xindex0].revents & IOPAUSE_READ)
          (*handle_stdin)(script, scriptlen, linelimit, gflags) ;
        else
        {
          if (indata.len) process_partial_line(script, scriptlen, gflags) ;
          prepare_to_exit() ;
        }
      }
    }
    finalize() ;
  }
  return 0 ;
}
