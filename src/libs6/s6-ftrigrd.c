/* ISC license. */

#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <regex.h>
#include <stdio.h>

#include <skalibs/posixplz.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr.h>
#include <skalibs/stralloc.h>
#include <skalibs/sig.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <skalibs/iopause.h>
#include <skalibs/sassserver.h>

#include "ftrigr-internal.h"

#include <skalibs/posixishard.h>

#define FTRIGRD_MAXREADS 32
#define FTRIGRD_BUFSIZE 32

#define dienomem() strerr_diefu1sys(111, "stralloc_catb")

typedef struct ftrigio_s ftrigio, *ftrigio_ref ;
struct ftrigio_s
{
  ftrigio_ref prev ;
  ftrigio_ref next ;
  unsigned int xindex ;
  uint32_t id ;
  uint32_t flags ;
  buffer b ;
  char buf[FTRIGRD_BUFSIZE] ;
  int fdw ;
  size_t start ;
  stralloc sa ;
  regex_t re ;
} ;
#define FTRIGIO_ZERO { .prev = 0, .next = 0, .xindex = 0, .id = 0, .flags = 0, .b = BUFFER_INIT(0, -1, 0, 0), .buf = "", .fdw = -1, .start = 0, .sa = STRALLOC_ZERO }

static ftrigio ftrigio_head = { .prev = &ftrigio_head, .next = &ftrigio_head, .start = 0, .sa = STRALLOC_ZERO } ;
#define numio (ftrigio_head.start)

static void cleanup (void *x)
{
  (void)x ;
  for (ftrigio *p = ftrigio_head.next ; p != &ftrigio_head ; p = p->next)
    unlink_void(p->sa.s) ;
}

static void ftrigio_remove (void *x)
{
  ftrigio *p = x ;
  p->next->prev = p->prev ;
  p->prev->next = p->next ;
  numio-- ;
  p->prev = p->next = 0 ;
  unlink_void(p->sa.s) ;
  fd_close(p->fdw) ;
  fd_close(buffer_fd(&p->b)) ;
  p->sa.len = 0 ;
  regfree(&p->re) ;
}

static int ftrigio_add (void *x, uint32_t id, uint32_t flags, uint32_t opcode, char const *s, size_t len)
{
  ftrigio *p = x ;
  size_t pathlen ;
  if (len < 3 || s[len - 1]) return EPROTO ;
  pathlen = strlen(s) ;
  if (pathlen + 1 >= len) return EPROTO ;
  p->start = pathlen + 41 ;
  if (!stralloc_ready(&p->sa, p->start)) { cleanup(0) ; dienomem() ; }

  {
    char tmp[p->start + 1] ;
    int r = skalibs_regcomp(&p->re, s + pathlen + 1, REG_EXTENDED) ;
    if (r) return r == REG_ESPACE ? ENOMEM : EINVAL ;
    memcpy(tmp, s, pathlen) ;
    memcpy(tmp + pathlen, "/.ftrig1:", 9) ;
    if (!timestamp(tmp + pathlen + 9)) goto err0 ;
    memcpy(tmp + pathlen + 34, ":XXXXXX", 8) ;
    r = mkptemp3(tmp, 0622, O_NONBLOCK | O_CLOEXEC) ;
    if (r == -1) goto err0 ;
    buffer_init(&p->b, &buffer_read, r, p->buf, FTRIGRD_BUFSIZE) ;
    p->fdw = open_write(tmp) ;
    if (p->fdw == -1) { unlink_void(tmp) ; goto err1 ; }
    stralloc_copyb(&p->sa, tmp, pathlen + 1) ;
    stralloc_catb(&p->sa, tmp + pathlen + 2, 40) ;
    if (rename(tmp, p->sa.s) == -1) { unlink_void(tmp) ; goto err2 ; }
  }
  
  p->id = id ;
  p->flags = flags ;
  p->prev = &ftrigio_head ;
  p->next = ftrigio_head.next ;
  ftrigio_head.next = p ;
  numio++ ;
  (void)opcode ;
  return 0 ;

 err2:
   fd_close(p->fdw) ;
 err1:
   fd_close(buffer_fd(&p->b)) ;
 err0:
  regfree(&p->re) ;
  return errno ;
}

static inline int ftrigio_read (sassserver *a, ftrigio *p)
{
  unsigned int i = FTRIGRD_MAXREADS ;
  while (i--)
  {
    size_t blen ;
    ssize_t r = sanitize_read(buffer_fill(&p->b)) ;
    if (!r) break ;
    if (r == -1)
    {
      sassserver_async_failure(a, p->id, errno) ;
      return 0 ;
    }
    blen = buffer_len(&p->b) ;
    if (!stralloc_readyplus(&p->sa, blen+1)) { cleanup(0) ; dienomem() ; }
    buffer_getnofill(&p->b, p->sa.s + p->sa.len, blen) ;
    p->sa.len += blen ;
    p->sa.s[p->sa.len] = 0 ;
    if (!regexec(&p->re, p->sa.s + p->start, 0, 0, 0))
    {
      sassserver_async_success(a, p->id, p->flags, p->sa.s + p->start, p->sa.len - p->start) ;
      p->sa.len = p->start ;
    }
  }
  return 1 ;
}

int main (void)
{
  sassserver a = SASSSERVER_ZERO ;
  int r = 0 ;
  PROG = "s6-ftrigrd" ;

  if (ndelay_on(0) == -1 || ndelay_on(1) == -1)
    strerr_diefu1sys(111, "make fds nonblocking") ;
  if (!sig_altignore(SIGPIPE))
    strerr_diefu1sys(111, "ignore SIGPIPE") ;
  if (!tain_now_set_stopwatch_g())
    strerr_diefu1sys(111, "tain_now_set_stopwatch") ;

  {
    tain deadline ;
    tain_addsec_g(&deadline, 5) ;
    sassserver_init_frompipe_g(&a, FTRIGR_BANNER1, FTRIGR_BANNER2, &ftrigio_add, &ftrigio_remove, sizeof(ftrigio), &cleanup, 0, &deadline) ;
  }

  while (!r)
  {
    tain deadline = TAIN_INFINITE ;
    iopause_fd x[3 + numio] ;

    sassserver_prepare_iopause(&a, x, &deadline) ;

    for (ftrigio *p = ftrigio_head.next ; p != &ftrigio_head ; p = p->next, r++)
    {
      p->xindex = 3+r ;
      x[3+r].fd = buffer_fd(&p->b) ;
      x[3+r].events = IOPAUSE_READ ;
    }

    r = iopause_g(x, 3 + numio, &deadline) ;
    if (r == -1)
    {
      cleanup(0) ;
      strerr_diefu1sys(111, "iopause") ;
    }
    if (!r)
    {
      sassserver_timeout(&a) ;
      continue ;
    }

    sassserver_write_event(&a, x) ;

    for (ftrigio *p = ftrigio_head.next ; p != &ftrigio_head ; p = p->next) if (x[p->xindex].revents & IOPAUSE_READ)
    {
      if (!ftrigio_read(&a, p))
      {
        p = p->prev ;
        ftrigio_remove(p->next) ;
      }
    }

    r = sassserver_read_event(&a, x) ;
  }

  cleanup(0) ;
  _exit(0) ;
}
