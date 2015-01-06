/* ISC license. */

#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <skalibs/uint16.h>
#include <skalibs/uint32.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/error.h>
#include <skalibs/strerr2.h>
#include <skalibs/genalloc.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <skalibs/iopause.h>
#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>
#include <s6/s6lock.h>

#define USAGE "s6lockd lockdir"
#define X() strerr_dief1x(101, "internal inconsistency, please submit a bug-report.")

typedef struct s6lockio_s s6lockio_t, *s6lockio_t_ref ;
struct s6lockio_s
{
  unsigned int xindex ;
  unsigned int pid ;
  tain_t limit ;
  int p[2] ;
  uint16 id ; /* given by client */
} ;
#define S6LOCKIO_ZERO { 0, 0, TAIN_ZERO, { -1, -1 }, 0 }
static s6lockio_t const szero = S6LOCKIO_ZERO ;

static genalloc a = GENALLOC_ZERO ; /* array of s6lockio_t */

static void s6lockio_free (s6lockio_t_ref p)
{
  register int e = errno ;
  fd_close(p->p[1]) ;
  fd_close(p->p[0]) ;
  kill(p->pid, SIGTERM) ;
  *p = szero ;
  errno = e ;
}

static void cleanup (void)
{
  register unsigned int i = genalloc_len(s6lockio_t, &a) ;
  for (; i ; i--) s6lockio_free(genalloc_s(s6lockio_t, &a) + i - 1) ;
  genalloc_setlen(s6lockio_t, &a, 0) ;
}
 
static void trig (uint16 id, char e)
{
  char pack[3] ;
  unixmessage_t m = { .s = pack, .len = 3, .fds = 0, .nfds = 0 } ;
  uint16_pack_big(pack, id) ;
  pack[2] = e ;
  if (!unixmessage_put(unixmessage_sender_x, &m))
  {
    cleanup() ;
    strerr_diefu1sys(111, "build answer") ;
  }
}

static void answer (char c)
{
  unixmessage_t m = { .s = &c, .len = 1, .fds = 0, .nfds = 0 } ;
  if (!unixmessage_put(unixmessage_sender_1, &m))
  {
    cleanup() ;
    strerr_diefu1sys(111, "unixmessage_put") ;
  }
}

static void remove (unsigned int i)
{
  register unsigned int n = genalloc_len(s6lockio_t, &a) - 1 ;
  s6lockio_free(genalloc_s(s6lockio_t, &a) + i) ;
  genalloc_s(s6lockio_t, &a)[i] = genalloc_s(s6lockio_t, &a)[n] ;
  genalloc_setlen(s6lockio_t, &a, n) ;
}

static void handle_signals (void)
{
  for (;;)
  {
    switch (selfpipe_read())
    {
      case -1 : cleanup() ; strerr_diefu1sys(111, "selfpipe_read") ;
      case 0 : return ;
      case SIGTERM :
      case SIGQUIT :
      case SIGHUP :
      case SIGABRT :
      case SIGINT : cleanup() ; _exit(0) ;
      case SIGCHLD : wait_reap() ; break ;
      default : cleanup() ; X() ;
    }
  }
}

static int parse_protocol (unixmessage_t const *m, void *context)
{
  uint16 id ;
  if (m->len < 3 || m->nfds)
  {
    cleanup() ;
    strerr_dief1x(100, "invalid client request") ;
  }
  uint16_unpack_big(m->s, &id) ;
  switch (m->s[2])
  {
    case '>' : /* release */
    {
      register unsigned int i = genalloc_len(s6lockio_t, &a) ;
      for (; i ; i--) if (genalloc_s(s6lockio_t, &a)[i-1].id == id) break ;
      if (i)
      {
        remove(i-1) ;
        answer(0) ;
      }
      else answer(ENOENT) ;
      break ;
    }
    case '<' : /* lock path */
    {
      s6lockio_t f = S6LOCKIO_ZERO ;
      char const *cargv[3] = { S6LOCKD_HELPER_PROG, 0, 0 } ;
      char const *cenvp[2] = { 0, 0 } ;
      uint32 options, pathlen ;
      if (m->len < 23)
      {
        answer(EPROTO) ;
        break ;
      }
      uint32_unpack_big(m->s + 3, &options) ;
      tain_unpack(m->s + 7, &f.limit) ;
      uint32_unpack_big(m->s + 19, &pathlen) ;
      if (pathlen + 23 != m->len || m->s[m->len - 1])
      {
        answer(EPROTO) ;
        break ;
      }
      f.id = id ;
      m->s[21] = '.' ;
      m->s[22] = '/' ;
      cargv[1] = (char const *)m->s + 21 ;
      if (options & S6LOCK_OPTIONS_EX) cenvp[0] = "S6LOCK_EX=1" ;
      f.pid = child_spawn(cargv[0], cargv, cenvp, f.p, 2) ;
      if (!f.pid)
      {
        answer(errno) ;
        break ;
      }
      if (!genalloc_append(s6lockio_t, &a, &f))
      {
        s6lockio_free(&f) ;
        answer(errno) ;
        break ;
      }
      answer(0) ;
      break ;
    }
    default :
    {
      cleanup() ;
      strerr_dief1x(100, "invalid client request") ;
    }
  }
  (void)context ;
  return 1 ;
}

int main (int argc, char const *const *argv)
{
  tain_t deadline ;
  int sfd ;
  PROG = "s6lockd" ;
  
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  if (chdir(argv[1]) < 0) strerr_diefu2sys(111, "chdir to ", argv[1]) ;
  if (ndelay_on(0) < 0) strerr_diefu2sys(111, "ndelay_on ", "0") ;
  if (ndelay_on(1) < 0) strerr_diefu2sys(111, "ndelay_on ", "1") ;
  if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "ignore SIGPIPE") ;

  sfd = selfpipe_init() ;
  if (sfd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
  {
    sigset_t set ;
    sigemptyset(&set) ;
    sigaddset(&set, SIGCHLD) ;
    sigaddset(&set, SIGTERM) ;
    sigaddset(&set, SIGQUIT) ;
    sigaddset(&set, SIGHUP) ;
    sigaddset(&set, SIGABRT) ;
    sigaddset(&set, SIGINT) ;
    if (selfpipe_trapset(&set) < 0)
      strerr_diefu1sys(111, "trap signals") ;
  }
  
  tain_now_g() ;
  tain_addsec_g(&deadline, 2) ;

  if (!skaclient_server_01x_init_g(S6LOCK_BANNER1, S6LOCK_BANNER1_LEN, S6LOCK_BANNER2, S6LOCK_BANNER2_LEN, &deadline))
    strerr_diefu1sys(111, "sync with client") ;

  for (;;)
  {
    register unsigned int n = genalloc_len(s6lockio_t, &a) ;
    iopause_fd x[4 + n] ;
    unsigned int i = 0 ;
    int r ;

    tain_add_g(&deadline, &tain_infinite_relative) ;
    x[0].fd = 0 ; x[0].events = IOPAUSE_EXCEPT | IOPAUSE_READ ;
    x[1].fd = 1 ; x[1].events = IOPAUSE_EXCEPT | (unixmessage_sender_isempty(unixmessage_sender_1) ? 0 : IOPAUSE_WRITE ) ;
    x[2].fd = unixmessage_sender_fd(unixmessage_sender_x) ;
    x[2].events = IOPAUSE_EXCEPT | (unixmessage_sender_isempty(unixmessage_sender_x) ? 0 : IOPAUSE_WRITE) ;
    x[3].fd = sfd ; x[3].events = IOPAUSE_READ ;
    for (; i < n ; i++)
    {
      register s6lockio_t_ref p = genalloc_s(s6lockio_t, &a) + i ;
      x[4+i].fd = p->p[0] ;
      x[4+i].events = IOPAUSE_READ ;
      if (p->limit.sec.x && tain_less(&p->limit, &deadline)) deadline = p->limit ;
      p->xindex = 4+i ;
    }

    r = iopause_g(x, 4 + n, &deadline) ;
    if (r < 0)
    {
      cleanup() ;
      strerr_diefu1sys(111, "iopause") ;
    }

   /* timeout => seek and destroy */
    if (!r)
    {
      for (i = 0 ; i < n ; i++)
      {
        register s6lockio_t_ref p = genalloc_s(s6lockio_t, &a) + i ;
        if (p->limit.sec.x && !tain_future(&p->limit)) break ;
      }
      if (i < n)
      {
        trig(genalloc_s(s6lockio_t, &a)[i].id, ETIMEDOUT) ;
        remove(i) ;
      }
      continue ;
    }

   /* client closed */
    if ((x[0].revents | x[1].revents) & IOPAUSE_EXCEPT) break ;

   /* client is reading */
    if (x[1].revents & IOPAUSE_WRITE)
      if (!unixmessage_sender_flush(unixmessage_sender_1) && !error_isagain(errno))
      {
        cleanup() ;
        strerr_diefu1sys(111, "flush stdout") ;
      }
    if (x[2].revents & IOPAUSE_WRITE)
      if (!unixmessage_sender_flush(unixmessage_sender_x) && !error_isagain(errno))
      {
        cleanup() ;
        strerr_diefu1sys(111, "flush asyncout") ;
      }

   /* scan children for successes */
    for (i = 0 ; i < genalloc_len(s6lockio_t, &a) ; i++)
    {
      register s6lockio_t_ref p = genalloc_s(s6lockio_t, &a) + i ;
      if (p->p[0] < 0) continue ;
      if (x[p->xindex].revents & IOPAUSE_READ)
      {
        char c ;
        register int r = sanitize_read(fd_read(p->p[0], &c, 1)) ;
        if (!r) continue ;
        if (r < 0)
        {
          trig(p->id, errno) ;
          remove(i--) ;
        }
        else if (c != '!')
        {
          trig(p->id, EPROTO) ;
          remove(i--) ;
        }
        else
        {
          trig(p->id, 0) ;
          p->limit = tain_zero ;
        }
      }
    }

   /* signals arrived */
    if (x[3].revents & (IOPAUSE_READ | IOPAUSE_EXCEPT)) handle_signals() ;

   /* client is writing */
    if (!unixmessage_receiver_isempty(unixmessage_receiver_0) || x[0].revents & IOPAUSE_READ)
    {
      if (unixmessage_handle(unixmessage_receiver_0, &parse_protocol, 0) < 0)
      {
        if (errno == EPIPE) break ; /* normal exit */
        cleanup() ;
        strerr_diefu1sys(111, "handle messages from client") ;
      }
    }
  }
  cleanup() ;
  return 0 ;
}
