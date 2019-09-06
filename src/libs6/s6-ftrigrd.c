/* ISC license. */

#include <sys/uio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <regex.h>

#include <skalibs/posixplz.h>
#include <skalibs/posixishard.h>
#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/error.h>
#include <skalibs/strerr2.h>
#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/sig.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <skalibs/iopause.h>
#include <skalibs/textmessage.h>
#include <skalibs/textclient.h>

#include "ftrig1.h"
#include <s6/ftrigr.h>

#define FTRIGRD_MAXREADS 32
#define FTRIGRD_BUFSIZE 17

#define dienomem() strerr_diefu1sys(111, "stralloc_catb")

typedef struct ftrigio_s ftrigio_t, *ftrigio_t_ref ;
struct ftrigio_s
{
  unsigned int xindex ;
  ftrig1_t trig ;
  buffer b ;
  char buf[FTRIGRD_BUFSIZE] ;
  regex_t re ;
  stralloc sa ;
  uint32_t options ;
  uint16_t id ; /* given by client */
} ;
#define FTRIGIO_ZERO { .xindex = 0, .trig = FTRIG1_ZERO, .b = BUFFER_INIT(0, -1, 0, 0), .buf = "", .sa = STRALLOC_ZERO, .options = 0, .id = 0 }

static ftrigio_t a[FTRIGR_MAX] ;
static unsigned int n = 0 ;

static void ftrigio_deepfree (ftrigio_t *p)
{
  ftrig1_free(&p->trig) ;
  stralloc_free(&p->sa) ;
  regfree(&p->re) ;
}

static void cleanup (void)
{
  unsigned int i = 0 ;
  for (; i < n ; i++) ftrigio_deepfree(a + i) ;
  n = 0 ;
}

static void trig (uint16_t id, char what, char info)
{
  char pack[4] ;
  uint16_pack_big(pack, id) ;
  pack[2] = what ; pack[3] = info ;
  if (!textmessage_put(textmessage_sender_x, pack, 4))
  {
    cleanup() ;
    strerr_diefu1sys(111, "build answer") ;
  }
}

static void answer (char c)
{
  if (!textmessage_put(textmessage_sender_1, &c, 1))
  {
    cleanup() ;
    strerr_diefu1sys(111, "textmessage_put") ;
  }
}

static void remove (unsigned int i)
{
  ftrigio_deepfree(a + i) ;
  a[i] = a[--n] ;
  a[i].b.c.x = a[i].buf ;
}

static inline int ftrigio_read (ftrigio_t *p)
{
  unsigned int i = FTRIGRD_MAXREADS ;
  while (i--)
  {
    regmatch_t pmatch ;
    size_t blen ;
    ssize_t r = sanitize_read(buffer_fill(&p->b)) ;
    if (!r) break ;
    if (r < 0) return (trig(p->id, 'd', errno), 0) ;
    blen = buffer_len(&p->b) ;
    if (!stralloc_readyplus(&p->sa, blen+1)) dienomem() ;
    buffer_getnofill(&p->b, p->sa.s + p->sa.len, blen) ;
    p->sa.len += blen ;
    p->sa.s[p->sa.len] = 0 ;
    while (!regexec(&p->re, p->sa.s, 1, &pmatch, REG_NOTBOL | REG_NOTEOL))
    {
      trig(p->id, '!', p->sa.s[pmatch.rm_eo - 1]) ;
      if (!(p->options & FTRIGR_REPEAT)) return 0 ;
      memmove(p->sa.s, p->sa.s + pmatch.rm_eo, p->sa.len + 1 - pmatch.rm_eo) ;
      p->sa.len -= pmatch.rm_eo ;
    }
  }
  return 1 ;
}

static int parse_protocol (struct iovec const *v, void *context)
{
  char const *s = v->iov_base ;
  uint16_t id ;
  if (v->iov_len < 3)
  {
    cleanup() ;
    strerr_dief1x(100, "invalid client request") ;
  }
  uint16_unpack_big(s, &id) ;
  switch (s[2])
  {
    case 'U' : /* unsubscribe */
    {
      unsigned int i = 0 ;
      for (; i < n ; i++) if (a[i].id == id) break ;
      if (i < n)
      {
        remove(i) ;
        answer(0) ;
      }
      else answer(EINVAL) ;
      break ;
    }
    case 'L' : /* subscribe to path and match re */
    {
      uint32_t options, pathlen, relen ;
      int r ;
      if (v->iov_len < 19)
      {
        answer(EPROTO) ;
        break ;
      }
      uint32_unpack_big(s + 3, &options) ;
      uint32_unpack_big(s + 7, &pathlen) ;
      uint32_unpack_big(s + 11, &relen) ;
      if (((pathlen + relen + 17) != v->iov_len) || s[15 + pathlen] || s[v->iov_len - 1])
      {
        answer(EPROTO) ;
        break ;
      }
      if (n >= FTRIGR_MAX)
      {
        answer(ENFILE) ;
        break ;
      }
      r = skalibs_regcomp(&a[n].re, s + 16 + pathlen, REG_EXTENDED) ;
      if (r)
      {
        answer(r == REG_ESPACE ? ENOMEM : EINVAL) ;
        break ;
      }
      if (!ftrig1_make(&a[n].trig, s + 15))
      {
        regfree(&a[n].re) ;
        answer(errno) ;
        break ;
      }
      buffer_init(&a[n].b, &buffer_read, a[n].trig.fd, a[n].buf, FTRIGRD_BUFSIZE) ;
      a[n].options = options ;
      a[n].id = id ;
      a[n].sa = stralloc_zero ;
      n++ ;
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

int main (void)
{
  PROG = "s6-ftrigrd" ;

  if (ndelay_on(0) < 0) strerr_diefu2sys(111, "ndelay_on ", "0") ;
  if (ndelay_on(1) < 0) strerr_diefu2sys(111, "ndelay_on ", "1") ;
  if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "ignore SIGPIPE") ;

  {
    tain_t deadline ;
    tain_now_set_stopwatch_g() ;
    tain_addsec_g(&deadline, 2) ;
    if (!textclient_server_01x_init_g(FTRIGR_BANNER1, FTRIGR_BANNER1_LEN, FTRIGR_BANNER2, FTRIGR_BANNER2_LEN, &deadline))
      strerr_diefu1sys(111, "sync with client") ;
  }

  for (;;)
  {
    iopause_fd x[3 + n] ;
    unsigned int i = 0 ;

    x[0].fd = 0 ; x[0].events = IOPAUSE_EXCEPT | IOPAUSE_READ ;
    x[1].fd = 1 ; x[1].events = IOPAUSE_EXCEPT | (textmessage_sender_isempty(textmessage_sender_1) ? 0 : IOPAUSE_WRITE) ;
    x[2].fd = textmessage_sender_fd(textmessage_sender_x) ;
    x[2].events = IOPAUSE_EXCEPT | (textmessage_sender_isempty(textmessage_sender_x) ? 0 : IOPAUSE_WRITE) ;
    for (; i < n ; i++)
    {
      a[i].xindex = 3 + i ;
      x[3+i].fd = a[i].trig.fd ;
      x[3+i].events = IOPAUSE_READ ;
    }

    if (iopause(x, 3 + n, 0, 0) < 0)
    {
      cleanup() ;
      strerr_diefu1sys(111, "iopause") ;
    }

   /* client closed */
    if ((x[0].revents | x[1].revents) & IOPAUSE_EXCEPT) break ;

   /* client is reading */
    if (x[1].revents & IOPAUSE_WRITE)
      if (!textmessage_sender_flush(textmessage_sender_1) && !error_isagain(errno))
      {
        cleanup() ;
        strerr_diefu1sys(111, "flush stdout") ;
      }
    if (x[2].revents & IOPAUSE_WRITE)
      if (!textmessage_sender_flush(textmessage_sender_x) && !error_isagain(errno))
      {
        cleanup() ;
        return 1 ;
      }

   /* scan listening ftrigs */
    for (i = 0 ; i < n ; i++)
    {
      if (x[a[i].xindex].revents & IOPAUSE_READ)
        if (!ftrigio_read(a+i)) remove(i--) ;
    }

   /* client is writing */
    if (!textmessage_receiver_isempty(textmessage_receiver_0) || x[0].revents & IOPAUSE_READ)
    {
      if (textmessage_handle(textmessage_receiver_0, &parse_protocol, 0) < 0)
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
