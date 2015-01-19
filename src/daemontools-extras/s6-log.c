/* ISC license. */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>  /* for rename() */
#include <stdlib.h>  /* for qsort() */
#include <regex.h>
#include <skalibs/uint32.h>
#include <skalibs/uint64.h>
#include <skalibs/uint.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/fmtscan.h>
#include <skalibs/bufalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/tai.h>
#include <skalibs/error.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/direntry.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/skamisc.h>
#include <skalibs/environ.h>
#include <execline/config.h>

#define USAGE "s6-log [ -q | -v ] [ -b ] [ -p ] [ -t ] [ -e ] logging_script"
#define dienomem() strerr_diefu1sys(111, "stralloc_catb")

static int flagstampalert = 0 ;
static int flagstamp = 0 ;
static int flagprotect = 0 ;
static int flagexiting = 0 ;
static unsigned int verbosity = 1 ;

static stralloc indata = STRALLOC_ZERO ;


/* Begin datatypes. Get ready for some lulz. */

typedef int qcmpfunc_t (void const *, void const *) ;
typedef qcmpfunc_t *qcmpfunc_t_ref ;

typedef enum rotstate_e rotstate_t, *rotstate_t_ref ;
enum rotstate_e
{
  ROTSTATE_WRITABLE,
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

static void sel_free (sel_t_ref s)
{
  if (s->type != SELTYPE_DEFAULT) regfree(&s->re) ;
  s->type = SELTYPE_PHAIL ;
}

typedef enum acttype_e acttype_t, *acttype_t_ref ;
enum acttype_e
{
  ACTTYPE_FD2,
  ACTTYPE_STATUS,
  ACTTYPE_DIR,
  ACTTYPE_PHAIL
} ;

typedef struct as_fd2_s as_fd2_t, *as_fd2_t_ref ;
struct as_fd2_s
{
  unsigned int size ;
} ;

typedef struct as_status_s as_status_t, *as_status_t_ref ;
struct as_status_s
{
  stralloc content ;
  char const *file ;
} ;

static void as_status_free (as_status_t_ref ap)
{
  stralloc_free(&ap->content) ;
  ap->file = 0 ;
}

typedef struct as_dir_s as_dir_t, *as_dir_t_ref ;
struct as_dir_s
{
  unsigned int lindex ;
} ;

typedef union actstuff_u actstuff_t, *actstuff_t_ref ;
union actstuff_u
{
  as_fd2_t fd2 ;
  as_status_t status ;
  as_dir_t dir ;
} ;

typedef struct act_s act_t, *act_t_ref ;
struct act_s
{
  acttype_t type ;
  actstuff_t data ;
} ;

static void act_free (act_t_ref ap)
{
  switch (ap->type)
  {
    case ACTTYPE_FD2 :
      break ;
    case ACTTYPE_STATUS :
      as_status_free(&ap->data.status) ;
      break ;
    case ACTTYPE_DIR :
      break ;
    default : break ;
  }
  ap->type = ACTTYPE_PHAIL ;
}

typedef struct scriptelem_s scriptelem_t, *scriptelem_t_ref ;
struct scriptelem_s
{
  genalloc selections ; /* array of sel_t */
  genalloc actions ; /* array of act_t */
} ;

#define SCRIPTELEM_ZERO { .selections = GENALLOC_ZERO, .actions = GENALLOC_ZERO }

static void scriptelem_free (scriptelem_t_ref se)
{
  scriptelem_t zero = SCRIPTELEM_ZERO ;
  genalloc_deepfree(sel_t, &se->selections, &sel_free) ;
  genalloc_deepfree(act_t, &se->actions, &act_free) ;
  *se = zero ;
}

typedef void inputprocfunc_t (scriptelem_t const *, unsigned int) ;
typedef inputprocfunc_t *inputprocfunc_t_ref ;

typedef struct logdir_s logdir_t, *logdir_t_ref ;
struct logdir_s
{
  bufalloc out ;
  tain_t retrytto ;
  tain_t deadline ;
  uint64 maxdirsize ;
  uint32 b ;
  uint32 n ;
  uint32 s ;
  uint32 tolerance ;
  unsigned int pid ;
  char const *dir ;
  char const *processor ;
  int fd ;
  int fdlock ;
  rotstate_t rstate ;
} ;

#define LOGDIR_ZERO { \
  .out = BUFALLOC_ZERO, \
  .retrytto = TAIN_ZERO, \
  .deadline = TAIN_ZERO, \
  .maxdirsize = 0, \
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

 /* If freeing a logdir before exiting is ever needed:
static void logdir_free (logdir_t_ref ldp)
{
  bufalloc_free(&ldp->out) ;
  fd_close(ldp->fd) ; ldp->fd = -1 ;
  fd_close(ldp->fdlock) ; ldp->fdlock = -1 ;
}
 */

/* End datatypes. All of this was just to optimize the script interpretation. :-) */

static genalloc logdirs = GENALLOC_ZERO ; /* array of logdir_t */

typedef struct filesize_s filesize_t, *filesize_t_ref ;
struct filesize_s
{
  uint64 size ;
  char name[28] ;
} ;

static int filesize_cmp (filesize_t const *a, filesize_t const *b)
{
  return byte_diff(a->name+1, 26, b->name+1) ;
}

static int name_is_relevant (char const *name)
{
  if (name[0] != '@') return 0 ;
  if (str_len(name) != 27) return 0 ;
  {
    char tmp[12] ;
    if (!ucharn_scan(name+1, tmp, 12)) return 0 ;
  }
  if (name[25] != '.') return 0 ;
  if ((name[26] != 's') && (name[26] != 'u')) return 0 ;
  return 1 ;
}

static inline int logdir_trim (logdir_t_ref ldp)
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
    register int e = errno ;
    dir_close(dir) ;
    errno = e ;
    return -1 ;
  }
  rewinddir(dir) ;
  {
    filesize_t blurgh[n] ;
    uint64 totalsize = 0 ;
    unsigned int dirlen = str_len(ldp->dir) ;
    unsigned int i = 0 ;
    char fullname[dirlen + 29] ;
    byte_copy(fullname, dirlen, ldp->dir) ;
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
      byte_copy(fullname + dirlen + 1, 28, d->d_name) ;
      if (stat(fullname, &st) < 0)
      {
        if (verbosity) strerr_warnwu2sys("stat ", fullname) ;
        continue ;
      }
      byte_copy(blurgh[i].name, 28, d->d_name) ;
      blurgh[i].size = st.st_size ;
      totalsize += st.st_size ;
      i++ ;
    }
    if (errno)
    {
      register int e = errno ;
      dir_close(dir) ;
      errno = e ;
      return -1 ;
    }
    dir_close(dir) ;
    if ((i <= ldp->n) && (!ldp->maxdirsize || (totalsize <= ldp->maxdirsize)))
      return 0 ;
    qsort(blurgh, i, sizeof(filesize_t), (qcmpfunc_t_ref)&filesize_cmp) ;
    n = 0 ;
    while ((i > ldp->n + n) || (ldp->maxdirsize && (totalsize > ldp->maxdirsize)))
    {
      byte_copy(fullname + dirlen + 1, 28, blurgh[n].name) ;
      if (unlink(fullname) < 0)
      {
        if (errno == ENOENT) totalsize -= blurgh[n].size ;
        if (verbosity) strerr_warnwu2sys("unlink ", fullname) ;
      }
      else totalsize -= blurgh[n].size ;
      n++ ;
    }
  }
  return n ;
}

static int finish (logdir_t *ldp, char const *name, char suffix)
{
  struct stat st ;
  unsigned int dirlen = str_len(ldp->dir) ;
  unsigned int namelen = str_len(name) ;
  char x[dirlen + namelen + 2] ;
  byte_copy(x, dirlen, ldp->dir) ;
  x[dirlen] = '/' ;
  byte_copy(x + dirlen + 1, namelen + 1, name) ;
  if (stat(x, &st) < 0) return (errno == ENOENT) ;
  if (st.st_nlink == 1)
  {
    char y[dirlen + 29] ;
    byte_copy(y, dirlen, ldp->dir) ;
    y[dirlen] = '/' ;
    timestamp_g(y + dirlen + 1) ;
    y[dirlen + 26] = '.' ;
    y[dirlen + 27] = suffix ;
    y[dirlen + 28] = 0 ;
    if (link(x, y) < 0) return 0 ;
  }
  if (unlink(x) < 0) return 0 ;
  return logdir_trim(ldp) ;
}

static inline void exec_processor (logdir_t_ref ldp)
{
  char const *cargv[4] = { EXECLINE_EXTBINPREFIX "execlineb", "-Pc", ldp->processor, 0 } ;
  unsigned int dirlen = str_len(ldp->dir) ;
  int fd ;
  char x[dirlen + 10] ;
  PROG = "s6-log (processor child)" ;
  byte_copy(x, dirlen, ldp->dir) ;
  byte_copy(x + dirlen, 10, "/previous") ;
  fd = open_readb(x) ;
  if (fd < 0) strerr_diefu2sys(111, "open_readb ", x) ;
  if (fd_move(0, fd) < 0) strerr_diefu2sys(111, "fd_move ", x) ;
  byte_copy(x + dirlen + 1, 10, "processed") ;
  fd = open_trunc(x) ;
  if (fd < 0) strerr_diefu2sys(111, "open_trunc ", x) ;
  if (fd_move(1, fd) < 0) strerr_diefu2sys(111, "fd_move ", x) ;
  byte_copy(x + dirlen + 1, 6, "state") ;
  fd = open_readb(x) ;
  if (fd < 0) strerr_diefu2sys(111, "open_readb ", x) ;
  if (fd_move(4, fd) < 0) strerr_diefu2sys(111, "fd_move ", x) ;
  byte_copy(x + dirlen + 1, 9, "newstate") ;
  fd = open_trunc(x) ;
  if (fd < 0) strerr_diefu2sys(111, "open_trunc ", x) ;
  if (fd_move(5, fd) < 0) strerr_diefu2sys(111, "fd_move ", x) ;
  selfpipe_finish() ;
  sig_restore(SIGPIPE) ;
  pathexec_run(cargv[0], cargv, (char const *const *)environ) ;
  strerr_dieexec(111, cargv[0]) ;
}

static int rotator (logdir_t_ref ldp)
{
  unsigned int dirlen = str_len(ldp->dir) ;
  switch (ldp->rstate)
  {
    case ROTSTATE_START :
    {
      if (fd_sync(ldp->fd) < 0)
      {
        if (verbosity) strerr_warnwu3sys("fd_sync ", ldp->dir, "/current") ;
        goto fail ;
      }
      tain_now_g() ;
      ldp->rstate = ROTSTATE_RENAME ;
    }
    case ROTSTATE_RENAME :
    {
      char current[dirlen + 9] ;
      char previous[dirlen + 10] ;
      byte_copy(current, dirlen, ldp->dir) ;
      byte_copy(current + dirlen, 9, "/current") ;
      byte_copy(previous, dirlen, ldp->dir) ;
      byte_copy(previous + dirlen, 10, "/previous") ;
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
      byte_copy(x, dirlen, ldp->dir) ;
      byte_copy(x + dirlen, 9, "/current") ;
      fd = open_append(x) ;
      if (fd < 0)
      {
        if (verbosity) strerr_warnwu2sys("open_append ", x) ;
        goto fail ;
      }
      if (coe(fd) < 0)
      {
        register int e = errno ;
        fd_close(fd) ;
        errno = e ;
        if (verbosity) strerr_warnwu2sys("coe ", x) ;
        goto fail ;
      }
      if (fd_chmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) < 0)
      {
        register int e = errno ;
        fd_close(fd) ;
        errno = e ;
        if (verbosity) strerr_warnwu3sys("fchmod ", x, " to 0644") ;
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
      byte_copy(x, dirlen, ldp->dir) ;
      byte_copy(x + dirlen, 10, "/previous") ;
      if (chmod(x, S_IRWXU | S_IRGRP | S_IROTH) < 0)
      {
        if (verbosity) strerr_warnwu3sys("chmod ", x, " to 0744") ;
        goto fail ;
      }
      if (ldp->processor) goto runprocessor ;
      ldp->rstate = ROTSTATE_FINISHPREVIOUS ;
    }
    case ROTSTATE_FINISHPREVIOUS :
    {
      if (finish(ldp, "previous", 's') < 0)
      {
        if (verbosity) strerr_warnwu2sys("finish previous .s to logdir ", ldp->dir) ;
        goto fail ;
      }
      tain_copynow(&ldp->deadline) ;
      ldp->rstate = ROTSTATE_WRITABLE ;
      break ;
    }
   runprocessor :
      ldp->rstate = ROTSTATE_RUNPROCESSOR ;
    case ROTSTATE_RUNPROCESSOR :
    {
      int pid = fork() ;
      if (pid < 0)
      {
        if (verbosity) strerr_warnwu2sys("fork processor for logdir ", ldp->dir) ;
        goto fail ;
      }
      else if (!pid) exec_processor(ldp) ;
      ldp->pid = (unsigned int)pid ;
      tain_add_g(&ldp->deadline, &tain_infinite_relative) ;
      ldp->rstate = ROTSTATE_WAITPROCESSOR ;
    }
    case ROTSTATE_WAITPROCESSOR :
    {
      return (errno = EAGAIN, 0) ;
    }
    case ROTSTATE_SYNCPROCESSED :
    {
      int fd ;
      char x[dirlen + 11] ;
      byte_copy(x, dirlen, ldp->dir) ;
      byte_copy(x + dirlen, 11, "/processed") ;
      fd = open_append(x) ;
      if (fd < 0)
      {
        if (verbosity) strerr_warnwu2sys("open_append ", x) ;
        goto fail ;
      }
      if (fd_sync(fd) < 0)
      {
        register int e = errno ;
        fd_close(fd) ;
        errno = e ;
        if (verbosity) strerr_warnwu2sys("fd_sync ", x) ;
        goto fail ;
      }
      tain_now_g() ;
      if (fd_chmod(fd, S_IRWXU | S_IRGRP | S_IROTH) < 0)
      {
        register int e = errno ;
        fd_close(fd) ;
        errno = e ;
        if (verbosity) strerr_warnwu3sys("fd_chmod ", x, " to 0744") ;
        goto fail ;
      }
      fd_close(fd) ;
      ldp->rstate = ROTSTATE_SYNCNEWSTATE ;
    }
    case ROTSTATE_SYNCNEWSTATE :
    {
      int fd ;
      char x[dirlen + 10] ;
      byte_copy(x, dirlen, ldp->dir) ;
      byte_copy(x + dirlen, 10, "/newstate") ;
      fd = open_append(x) ;
      if (ldp->fd < 0)
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
      byte_copy(x, dirlen, ldp->dir) ;
      byte_copy(x + dirlen, 10, "/previous") ;
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
      byte_copy(newstate, dirlen, ldp->dir) ;
      byte_copy(state, dirlen, ldp->dir) ;
      byte_copy(newstate + dirlen, 10, "/newstate") ;
      byte_copy(state + dirlen, 7, "/state") ;
      if (rename(newstate, state) < 0)
      {
        if (verbosity) strerr_warnwu4sys("rename ", newstate, " to ", state) ;
        goto fail ;
      }
      ldp->rstate = ROTSTATE_FINISHPROCESSED ;
    }
    case ROTSTATE_FINISHPROCESSED :
    {
      if (finish(ldp, "processed", 's') < 0)
      {
        if (verbosity) strerr_warnwu2sys("finish processed .s to logdir ", ldp->dir) ;
        goto fail ;
      }
      tain_copynow(&ldp->deadline) ;
      ldp->rstate = ROTSTATE_WRITABLE ;
      break ;
    }
    default : strerr_dief1x(101, "inconsistent state in rotator()") ;
  }
  return 1 ;
 fail:
   tain_add_g(&ldp->deadline, &ldp->retrytto) ;
   return 0 ;
}

static int logdir_write (int i, char const *s, unsigned int len)
{
  logdir_t_ref ldp = genalloc_s(logdir_t, &logdirs) + (unsigned int)i ;
  int r ;
  unsigned int n = len ;
  {
    unsigned int m = byte_rchr(s, n, '\n') ;
    if (m < n) n = m+1 ;
  }
  r = fd_write(ldp->fd, s, n) ;
  if (r < 0)
  {
    if (!error_isagain(errno))
    {
      tain_add_g(&ldp->deadline, &ldp->retrytto) ;
      if (verbosity) strerr_warnwu3sys("write to ", ldp->dir, "/current") ;
    }
    return r ;
  }
  ldp->b += r ;
  if ((ldp->b + ldp->tolerance >= ldp->s) && (s[r-1] == '\n'))
  {
    ldp->rstate = ROTSTATE_START ;
    rotator(ldp) ;
  }
  return r ;
}

static inline void rotate_or_flush (logdir_t *ldp)
{
  if ((ldp->rstate != ROTSTATE_WRITABLE) && !rotator(ldp)) return ;
  if (ldp->b >= ldp->s)
  {
    ldp->rstate = ROTSTATE_START ;
    if (!rotator(ldp)) return ;
  }
  bufalloc_flush(&ldp->out) ;
}

static inline void logdir_init (logdir_t *ap, uint32 s, uint32 n, uint32 tolerance, uint64 maxdirsize, tain_t const *retrytto, char const *processor, char const *name, unsigned int index)
{
  struct stat st ;
  unsigned int dirlen = str_len(name) ;
  int r ;
  char x[dirlen + 11] ;
  ap->s = s ;
  ap->n = n ;
  ap->pid = 0 ;
  ap->tolerance = tolerance ;
  ap->maxdirsize = maxdirsize ;
  ap->retrytto = *retrytto ;
  ap->processor = processor ;
  ap->dir = name ;
  ap->fd = -1 ;
  ap->rstate = ROTSTATE_WRITABLE ;
  r = mkdir(ap->dir, S_IRWXU | S_ISGID) ;
  if ((r < 0) && (errno != EEXIST)) strerr_diefu2sys(111, "mkdir ", name) ;
  byte_copy(x, dirlen, name) ;
  byte_copy(x + dirlen, 6, "/lock") ;
  ap->fdlock = open_append(x) ;
  if ((ap->fdlock) < 0) strerr_diefu2sys(111, "open_append ", x) ;
  if (lock_exnb(ap->fdlock) < 0) strerr_diefu2sys(111, "lock_exnb ", x) ;
  if (coe(ap->fdlock) < 0) strerr_diefu2sys(111, "coe ", x) ;
  byte_copy(x + dirlen + 1, 8, "current") ;
  if (stat(x, &st) < 0)
  {
    if (errno != ENOENT) strerr_diefu2sys(111, "stat ", x) ;
  }
  else if (st.st_mode & S_IXUSR) goto opencurrent ;
  byte_copy(x + dirlen + 1, 6, "state") ;
  unlink(x) ;
  byte_copy(x + dirlen + 1, 9, "newstate") ;
  unlink(x) ;
  {
    int flagprocessed = 0 ;
    byte_copy(x + dirlen + 1, 10, "processed") ;
    if (stat(x, &st) < 0)
    {
      if (errno != ENOENT) strerr_diefu2sys(111, "stat ", x) ;
    }
    else if (st.st_mode & S_IXUSR) flagprocessed = 1 ;
    if (flagprocessed)
    {
      byte_copy(x + dirlen + 1, 9, "previous") ;
      unlink(x) ;
      if (finish(ap, "processed", 's') < 0)
        strerr_diefu2sys(111, "finish processed .s for logdir ", ap->dir) ;
    }
    else
    {
      unlink(x) ;
      if (finish(ap, "previous", 'u') < 0)
        strerr_diefu2sys(111, "finish previous .u for logdir ", ap->dir) ;
    }
  }
  if (finish(ap, "current", 'u') < 0)
    strerr_diefu2sys(111, "finish current .u for logdir ", ap->dir) ;
  byte_copy(x + dirlen + 1, 6, "state") ;
  ap->fd = open_trunc(x) ;
  if (ap->fd < 0) strerr_diefu2sys(111, "open_trunc ", x) ;
  fd_close(ap->fd) ;
  st.st_size = 0 ;
  byte_copy(x + dirlen + 1, 8, "current") ;
 opencurrent:
  ap->fd = open_append(x) ;
  if (ap->fd < 0) strerr_diefu2sys(111, "open_append ", x) ;
  if (fd_chmod(ap->fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) == -1)
    strerr_diefu2sys(111, "fd_chmod ", x) ;
  if (coe(ap->fd) < 0) strerr_diefu2sys(111, "coe ", x) ;
  ap->b = st.st_size ;
  tain_copynow(&ap->deadline) ;
  bufalloc_init(&ap->out, &logdir_write, (int)index) ;
}


 /* Script */
 
static int script_update (genalloc *sc, genalloc *sa, genalloc *aa)
{
  scriptelem_t foo ;
  genalloc_shrink(sel_t, sa) ;
  genalloc_shrink(act_t, aa) ;
  foo.selections = *sa ;
  foo.actions = *aa ;
  if (!genalloc_append(scriptelem_t, sc, &foo)) return 0 ;
  *sa = genalloc_zero ;
  *aa = genalloc_zero ;
  return 1 ;
}

static inline int script_init (genalloc *sc, char const *const *argv)
{
  tain_t cur_retrytto ;
  unsigned int cur_fd2_size = 200 ;
  unsigned int cur_status_size = 1001 ;
  uint32 cur_s = 99999 ;
  uint32 cur_n = 10 ;
  uint32 cur_tolerance = 2000 ;
  uint64 cur_maxdirsize = 0 ;
  genalloc cur_selections = GENALLOC_ZERO ; /* sel_t */
  genalloc cur_actions = GENALLOC_ZERO ; /* act_t */
  char const *cur_processor = 0 ;
  int flagacted = 0 ;
  tain_uint(&cur_retrytto, 2) ;
  
  for (; *argv ; argv++)
  {
    switch (**argv)
    {
      case 'f' :
      {
        sel_t selitem ;
        if (flagacted)
        {
          if (!script_update(sc, &cur_selections, &cur_actions)) return 0 ;
          flagacted = 0 ;
        }
        selitem.type = SELTYPE_DEFAULT ;
        if (!genalloc_append(sel_t, &cur_selections, &selitem)) return 0 ;
        break ;
      }
      case '+' :
      case '-' :
      {
        sel_t selitem ;
        int r ;
        if (flagacted)
        {
          if (!script_update(sc, &cur_selections, &cur_actions)) return 0 ;
          flagacted = 0 ;
        }
        selitem.type = (**argv == '+') ? SELTYPE_PLUS : SELTYPE_MINUS ;
        r = regcomp(&selitem.re, *argv+1, REG_EXTENDED | REG_NOSUB | REG_NEWLINE) ;
        if (r == REG_ESPACE) return (errno = ENOMEM, 0) ;
        if (r) goto fail ;
        if (!genalloc_append(sel_t, &cur_selections, &selitem)) return 0 ;
        break ;
      }
      case 'n' :
      {
        if (!uint320_scan(*argv + 1, &cur_n)) goto fail ;
        break ;
      }
      case 's' :
      {
        if (!uint320_scan(*argv + 1, &cur_s)) goto fail ;
        if (cur_s < 4096) cur_s = 4096 ;
        if (cur_s > 16777215) cur_s = 16777215 ;
        break ;
      }
      case 'S' :
      {
        if (!uint640_scan(*argv + 1, &cur_maxdirsize)) goto fail ;
        break ;
      }
      case 'l' :
      {
        if (!uint320_scan(*argv + 1, &cur_tolerance)) goto fail ;
        if (cur_tolerance > (cur_s >> 1))
          strerr_dief3x(100, "directive ", *argv, " conflicts with previous s directive") ;
        break ;
      }
      case 'r' :
      {
        uint32 t ;
        if (!uint320_scan(*argv + 1, &t)) goto fail ;
        if (!tain_from_millisecs(&cur_retrytto, (int)t)) return (errno = EINVAL, 0) ;
        break ;
      }
      case 'E' :
      {
        if (!uint0_scan(*argv + 1, &cur_fd2_size)) goto fail ;
        break ;
      }
      case '^' :
      {
        if (!uint0_scan(*argv + 1, &cur_status_size)) goto fail ;
        break ;
      }
      case '!' :
      {
        cur_processor = (*argv)[1] ? *argv + 1 : 0 ;
        break ;
      }
      case 'e' :
      {
        act_t a ;
        flagacted = 1 ;
        a.type = ACTTYPE_FD2 ;
        a.data.fd2.size = cur_fd2_size ;
        if (!genalloc_append(act_t, &cur_actions, &a)) return 0 ;
        break ;
      }
      case '=' :
      {
        act_t a ;
        flagacted = 1 ;
        a.type = ACTTYPE_STATUS ;
        a.data.status.file = *argv + 1 ;
        a.data.status.content = stralloc_zero ;
        if (cur_status_size && !stralloc_ready_tuned(&a.data.status.content, cur_status_size, 0, 0, 1)) return 0 ;
        a.data.status.content.len = cur_status_size ;
        if (!genalloc_append(act_t, &cur_actions, &a)) return 0 ;
        break ;
      }
      case '.' : 
      case '/' :
      {
        act_t a ;
        logdir_t ld = LOGDIR_ZERO ;
        flagacted = 1 ;
        a.type = ACTTYPE_DIR ;
        a.data.dir.lindex = genalloc_len(logdir_t, &logdirs) ;
        if (!genalloc_append(act_t, &cur_actions, &a)) return 0 ;
        logdir_init(&ld, cur_s, cur_n, cur_tolerance, cur_maxdirsize, &cur_retrytto, cur_processor, *argv, genalloc_len(logdir_t, &logdirs)) ;
        if (!genalloc_append(logdir_t, &logdirs, &ld)) return 0 ;
        break ;
      }
      default : goto fail ;
    }
  }
  if (flagacted)
  {
    if (!script_update(sc, &cur_selections, &cur_actions)) return 0 ;
  }
  else
  {
    genalloc_deepfree(sel_t, &cur_selections, &sel_free) ;
    if (verbosity) strerr_warnw1x("ignoring extraneous non-action directives") ;
  }
  genalloc_shrink(logdir_t, &logdirs) ;
  genalloc_shrink(scriptelem_t, sc) ;
  if (!genalloc_len(scriptelem_t, sc))
    strerr_dief1x(100, "no action directive specified") ;
  return 1 ;
 fail:
  strerr_dief2x(100, "unrecognized directive: ", *argv) ;
}

static inline void doit_fd2 (as_fd2_t const *ap, char const *s, unsigned int len)
{
  if (flagstampalert)
  {
    char fmt[TIMESTAMP+1] ;
    tain_now_g() ;
    timestamp_g(fmt) ;
    fmt[TIMESTAMP] = ' ' ;
    buffer_put(buffer_2, fmt, TIMESTAMP+1) ;
  }
  buffer_puts(buffer_2, PROG) ;
  buffer_puts(buffer_2, ": alert: ") ;
  if (ap->size && len > ap->size) len = ap->size ;
  buffer_put(buffer_2, s, len) ;
  if (len == ap->size) buffer_puts(buffer_2, "...") ;
  buffer_putflush(buffer_2, "\n", 1) ;
}

static inline void doit_status (as_status_t const *ap, char const *s, unsigned int len)
{
  if (ap->content.len)
  {
    register unsigned int i ;
    if (len > ap->content.len) len = ap->content.len ;
    byte_copy(ap->content.s, len, s) ;
    for (i = len ; i < ap->content.len ; i++) ap->content.s[i] = '\n' ;
    if (!openwritenclose_suffix_sync(ap->file, ap->content.s, ap->content.len, ".new"))
      strerr_warnwu2sys("openwritenclose ", ap->file) ;
  }
  else if (!openwritenclose_suffix_sync(ap->file, s, len, ".new"))
    strerr_warnwu2sys("openwritenclose ", ap->file) ;
}

static inline void doit_dir (as_dir_t const *ap, char const *s, unsigned int len)
{
  logdir_t_ref ldp = genalloc_s(logdir_t, &logdirs) + ap->lindex ;
  if (!bufalloc_put(&ldp->out, s, len) || !bufalloc_put(&ldp->out, "\n", 1))
    strerr_diefu1sys(111, "bufalloc_put") ;
}


 /* The script interpreter. */

static inline void doit (scriptelem_t const *se, unsigned int n, char const *s, unsigned int len)
{
  int flagselected = 1 ;
  int flagacted = 0 ;
  unsigned int i = 0 ;
  for (; i < n ; i++)
  {
    unsigned int sellen = genalloc_len(sel_t, &se[i].selections) ;
    sel_t *sels = genalloc_s(sel_t, &se[i].selections) ;
    unsigned int j = 0 ;
    for (; j < sellen ; j++)
    {
      switch (sels[j].type)
      {
        case SELTYPE_DEFAULT :
          flagselected = !flagacted ;
          break ;
        case SELTYPE_PLUS :
	  if (!flagselected && !regexec(&sels[j].re, flagstamp ? s+TIMESTAMP+1 : s, 0, 0, 0)) flagselected = 1 ;
          break ;
        case SELTYPE_MINUS :
	  if (flagselected && !regexec(&sels[j].re, flagstamp ? s+TIMESTAMP+1 : s, 0, 0, 0)) flagselected = 0 ;
          break ;
        default :
          strerr_dief2x(101, "internal consistency error in ", "selection type") ;
      }
    }
    if (flagselected)
    {
      unsigned int actlen = genalloc_len(act_t, &se[i].actions) ;
      act_t *acts = genalloc_s(act_t, &se[i].actions) ;
      flagacted = 1 ;
      for (j = 0 ; j < actlen ; j++)
      {
        switch (acts[j].type)
        {
          case ACTTYPE_FD2 :
            doit_fd2(&acts[j].data.fd2, s, len) ;
            break ;
          case ACTTYPE_STATUS :
            doit_status(&acts[j].data.status, s, len) ;
            break ;
          case ACTTYPE_DIR :
            doit_dir(&acts[j].data.dir, s, len) ;
            break ;
          default :
            strerr_dief2x(101, "internal consistency error in ", "action type") ;
        }
      }
    }
  }
  if (flagstamp) tain_now_g() ;
}

static inline void processor_died (logdir_t_ref ldp, int wstat)
{
  ldp->pid = 0 ;
  if (WIFSIGNALED(wstat))
  {
    if (verbosity) strerr_warnw2x("processor crashed in ", ldp->dir) ;
    tain_add_g(&ldp->deadline, &ldp->retrytto) ;
    ldp->rstate = ROTSTATE_RUNPROCESSOR ;
  }
  else if (WEXITSTATUS(wstat))
  {
    if (verbosity) strerr_warnw2x("processor failed in ", ldp->dir) ;
    tain_add_g(&ldp->deadline, &ldp->retrytto) ;
    ldp->rstate = ROTSTATE_RUNPROCESSOR ;
  }
  else
  {
    ldp->rstate = ROTSTATE_SYNCPROCESSED ;
    rotator(ldp) ;
  }
}

static void prepare_to_exit (void)
{
  fd_close(0) ;
  flagexiting = 1 ;
}

static void stampanddoit (scriptelem_t const *se, unsigned int n)
{
  if (flagstamp) indata.s[timestamp_g(indata.s)] = ' ' ;
  indata.s[indata.len] = 0 ;
  doit(se, n, indata.s, indata.len-1) ;
  indata.len = flagstamp ? TIMESTAMP+1 : 0 ;
}

static void normal_stdin (scriptelem_t const *se, unsigned int selen)
{
  int r = sanitize_read(buffer_fill(buffer_0)) ;
  if (r < 0)
  {
    if ((errno != EPIPE) && verbosity) strerr_warnwu1sys("read from stdin") ;
    prepare_to_exit() ;
  }
  else if (r)
    while (skagetln_nofill(buffer_0, &indata, '\n') > 0)
      stampanddoit(se, selen) ;
}

static void last_stdin (scriptelem_t const *se, unsigned int selen)
{
  int cont = 1 ;
  while (cont)
  {
    char c ;
    switch (sanitize_read(fd_read(0, &c, 1)))
    {
      case 0 :
        cont = 0 ;
        break ;
      case -1 :
        if ((errno != EPIPE) && verbosity) strerr_warnwu1sys("read from stdin") ;
        if (indata.len <= (flagstamp ? TIMESTAMP+1 : 0))
        {
          prepare_to_exit() ;
          cont = 0 ;
          break ;
        }
        c = '\n' ;
      case 1 :
        if (!stralloc_catb(&indata, &c, 1)) dienomem() ;
        if (c == '\n')
        {
          stampanddoit(se, selen) ;
          prepare_to_exit() ;
          cont = 0 ;
        }
        break ;
    }
  }
}

static inputprocfunc_t_ref handle_stdin = &normal_stdin ;

static inline void handle_signals (void)
{
  for (;;)
  {
    switch (selfpipe_read())
    {
      case -1 : strerr_diefu1sys(111, "selfpipe_read") ;
      case 0 : return ;
      case SIGALRM :
      {
        unsigned int llen = genalloc_len(logdir_t, &logdirs) ;
        logdir_t *ls = genalloc_s(logdir_t, &logdirs) ;
        register unsigned int i = 0 ;
        for (i = 0 ; i < llen ; i++)
          if ((ls[i].rstate == ROTSTATE_WRITABLE) && ls[i].b)
          {
            ls[i].rstate = ROTSTATE_START ;
            rotator(ls + i) ;
          }
        break ;
      }
      case SIGTERM :
      {
        if (flagprotect) break ;
        handle_stdin = &last_stdin ;
        if (indata.len <= (flagstamp ? TIMESTAMP+1 : 0)) prepare_to_exit() ;
        break ;
      }
      case SIGCHLD :
      {
        unsigned int llen = genalloc_len(logdir_t, &logdirs) ;
        logdir_t *ls = genalloc_s(logdir_t, &logdirs) ;
        for (;;)
        {
          int wstat ;
          register unsigned int i = 0 ;
          register int r = wait_nohang(&wstat) ;
          if (r <= 0) break ;
          for (; i < llen ; i++) if ((unsigned int)r == ls[i].pid) break ;
          if (i < llen) processor_died(ls + i, wstat) ;
        }
        break ;
      }
      default : strerr_dief1x(101, "internal consistency error with signal handling") ;
    }
  }
}

static inline int logdir_finalize (logdir_t_ref ldp)
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
      if (fd_chmod(ldp->fd, S_IRWXU | S_IRGRP | S_IROTH) < 0)
      {
        if (verbosity) strerr_warnwu3sys("fd_chmod ", ldp->dir, "/current to 0744") ;
        goto fail ;
      }
      ldp->rstate = ROTSTATE_END ;
      break ;
    }
    default : strerr_dief1x(101, "inconsistent state in logdir_finalize()") ;
  }
  return 1 ;
 fail:
  tain_add_g(&ldp->deadline, &ldp->retrytto) ;
  return 0 ;
}

static inline void finalize (void)
{
  unsigned int llen = genalloc_len(logdir_t, &logdirs) ;
  logdir_t *ls = genalloc_s(logdir_t, &logdirs) ;
  unsigned int n = llen ;
  for (;;)
  {
    unsigned int i = 0 ;
    tain_t deadline ;
    tain_addsec_g(&deadline, 2) ;
    for (; i < llen ; i++)
      if (ls[i].rstate != ROTSTATE_END)
      {
        if (logdir_finalize(ls + i)) n-- ;
        else if (tain_less(&ls[i].deadline, &deadline))
          deadline = ls[i].deadline ;
      }
    if (!n) break ;
    {
      iopause_fd x ;
      iopause_g(&x, 0, &deadline) ;
    }
  }
}

int main (int argc, char const *const *argv)
{
  genalloc logscript = GENALLOC_ZERO ; /* array of scriptelem_t */
  int flagblock = 0 ;
  PROG = "s6-log" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "qvbpte", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'q' : if (verbosity) verbosity-- ; break ;
        case 'v' : verbosity++ ; break ;
        case 'b' : flagblock = 1 ; break ;
        case 'p' : flagprotect = 1 ; break ;
        case 't' : flagstamp = 1 ; break ;
        case 'e' : flagstampalert = 1 ; break ;
        default : strerr_dieusage(100, USAGE) ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (argc < 1) strerr_dieusage(100, USAGE) ;

  fd_close(1) ;
  {
    int r = tain_now_g() ;
    if (flagstamp)
    {
      char fmt[TIMESTAMP+1] ;
      if (!stralloc_catb(&indata, fmt, TIMESTAMP+1)) dienomem() ;
      if (!r) strerr_warnwu1sys("read current time - timestamps may be wrong for a while") ;
    }
  }
  if (!script_init(&logscript, argv)) strerr_diefu1sys(111, "initialize logging script") ;
  if (ndelay_on(0) < 0) strerr_diefu1sys(111, "ndelay_on(0)") ;

  {
    unsigned int llen = genalloc_len(logdir_t, &logdirs) ;
    logdir_t *ls = genalloc_s(logdir_t, &logdirs) ;
    iopause_fd x[2 + llen] ;
    unsigned int active[llen] ;
    x[0].fd = 0 ;
    x[1].fd = selfpipe_init() ;
    if (x[1].fd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
    if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "sig_ignore(SIGPIPE)") ;
    {
      sigset_t set ;
      sigemptyset(&set) ;
      sigaddset(&set, SIGTERM) ; sigaddset(&set, SIGALRM) ; sigaddset(&set, SIGCHLD) ;
      if (selfpipe_trapset(&set) < 0) strerr_diefu1sys(111, "selfpipe_trapset") ;
    }
    x[1].events = IOPAUSE_READ ;

    for (;;)
    {
      tain_t deadline ;
      int r ;
      unsigned int j = 0 ;
      unsigned int i = 0 ;
      int allflushed = 1 ;
      tain_add_g(&deadline, &tain_infinite_relative) ;
      for (; i < llen ; i++)
      {
        if (bufalloc_len(&ls[i].out) || (ls[i].rstate != ROTSTATE_WRITABLE))
        {
          allflushed = 0 ;
          if (!tain_future(&ls[i].deadline))
          {
            x[2+j].fd = ls[i].fd ;
            x[2+j].events = IOPAUSE_WRITE ;
            active[j++] = i ;
          }
          else if (tain_less(&ls[i].deadline, &deadline))
              deadline = ls[i].deadline ;
        }
      }
      if (flagexiting && allflushed) break ;
      x[0].events = (allflushed || !flagblock) ? IOPAUSE_READ : 0 ;
      r = iopause_g(x + flagexiting, 2 - flagexiting + j, &deadline) ;
      if (r < 0) strerr_diefu1sys(111, "iopause") ;
      else if (r)
      {
        if (x[1].revents & IOPAUSE_READ) handle_signals() ;
        else if (x[1].revents & IOPAUSE_EXCEPT) strerr_dief1sys(111, "trouble with selfpipe") ;
        for (i = 0 ; i < j ; i++)
          if (x[2+i].revents & IOPAUSE_WRITE)
            rotate_or_flush(ls + active[i]) ;
        if (!flagexiting)
        {
          if (x[0].revents & IOPAUSE_READ)
            (*handle_stdin)(genalloc_s(scriptelem_t, &logscript), genalloc_len(scriptelem_t, &logscript)) ;
          else if (x[0].revents & IOPAUSE_EXCEPT)
          {
            prepare_to_exit() ;
            if (indata.len > (flagstamp ? TIMESTAMP+1 : 0))
            {
              if (!stralloc_0(&indata)) dienomem() ;
              stampanddoit(genalloc_s(scriptelem_t, &logscript), genalloc_len(scriptelem_t, &logscript)) ;
            }
          }
        }
      }
    }
  }
  genalloc_deepfree(scriptelem_t, &logscript, &scriptelem_free) ;
  finalize() ;
  return 0 ;
}
