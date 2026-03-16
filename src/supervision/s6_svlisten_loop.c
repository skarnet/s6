/* ISC license. */

#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

#include <skalibs/bytestr.h>
#include <skalibs/bitarray.h>
#include <skalibs/strerr.h>
#include <skalibs/iopause.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/selfpipe.h>

#include <s6/ftrigw.h>
#include <s6/ftrigr.h>
#include <s6/supervise.h>
#include "s6-svlisten.h"

void s6_svlisten_init (int argc, char const *const *argv, s6_svlisten_t *foo, uint32_t *ids, unsigned char *upstate, unsigned char *readystate, tain const *deadline)
{
  gid_t gid = getegid() ;
  unsigned int i = 0 ;
  foo->n = (unsigned int)argc ;
  foo->ids = ids ;
  foo->upstate = upstate ;
  foo->readystate = readystate ;
  if (!ftrigr_startf_g(&foo->a, deadline)) strerr_diefu1sys(111, "ftrigr_startf") ;
  for (; i < foo->n ; i++)
  {
    s6_svstatus_t status = S6_SVSTATUS_ZERO ;
    size_t len = strlen(argv[i]) ;
    char s[len + 1 + sizeof(S6_SUPERVISE_EVENTDIR)] ;
    memcpy(s, argv[i], len) ;
    s[len] = '/' ;
    memcpy(s + len + 1, S6_SUPERVISE_EVENTDIR, sizeof(S6_SUPERVISE_EVENTDIR)) ;
    ftrigw_fifodir_make(s, gid, 0) ;
    if (!ftrigr_subscribe_g(&foo->a, foo->ids + i, FTRIGR_REPEAT, 0, s, "[DuUdOx]", deadline))
      strerr_diefu2sys(111, "subscribe to events for ", argv[i]) ;
    if (!s6_svstatus_read(argv[i], &status)) strerr_diefu1sys(111, "s6_svstatus_read") ;
    bitarray_poke(foo->upstate, i, status.pid && !status.flagfinishing) ;
    bitarray_poke(foo->readystate, i, status.flagready) ;
  }
}

static inline int got (s6_svlisten_t const *foo, int wantup, int wantready, int or)
{
  unsigned int m = bitarray_div8(foo->n) ;
  unsigned char t[m] ;
  memcpy(t, foo->upstate, m) ;
  if (!wantup) bitarray_not(t, 0, foo->n) ;
  if (wantready) bitarray_and(t, t, foo->readystate, foo->n) ;
  return (bitarray_first(t, foo->n, or) < foo->n) == or ;
}

unsigned int s6_svlisten_loop (s6_svlisten_t *foo, int wantup, int wantready, int or, tain const *deadline, int spfd, action_func_ref handler)
{
  iopause_fd x[2] = { { .fd = ftrigr_fd(&foo->a), .events = IOPAUSE_READ }, { .fd = spfd, .events = IOPAUSE_READ, .revents = 0 } } ;
  unsigned int e = 0 ;

  if (got(foo, wantup, wantready, or)) return 0 ;
  for (;;)
  {
    int r = iopause_g(x, 1 + (spfd >= 0), deadline) ;
    if (r < 0) strerr_diefu1sys(111, "iopause") ;
    else if (!r) strerr_dief1x(99, "timed out") ;
    if (x[1].revents & IOPAUSE_READ) (*handler)() ;
    if (x[0].revents & IOPAUSE_READ)
    {
      if (ftrigr_update(&foo->a) < 0) strerr_diefu1sys(111, "ftrigr_update") ;
      for (unsigned int i = 0 ; i < foo->n ; i++)
      {
        struct iovec v ;
        r = ftrigr_peek(&foo->a, foo->ids[i], &v) ;
        if (r == -1) strerr_diefu1sys(111, "ftrigr_check") ;
        else if (r)
        {
          char const *s = v.iov_base ;
          for (size_t j = 0 ; j < v.iov_len ; j++)
          {
            if (s[j] == 'x')
            {
              if (bitarray_peek(foo->upstate, i) != wantup
               || bitarray_peek(foo->readystate, i) != wantready)
                e++ ;
              bitarray_poke(foo->upstate, i, wantup) ;
              bitarray_poke(foo->readystate, i, wantready) ;
            }
            else if (s[j] == 'O')
            {
              if (wantup)
              {
                bitarray_poke(foo->upstate, i, wantup) ;
                bitarray_poke(foo->readystate, i, wantready) ;
                e++ ;
              }
            }
            else
            {
              unsigned int d = byte_chr("dDuU", 4, s[j]) ;
              bitarray_poke(foo->upstate, i, d & 2) ;
              bitarray_poke(foo->readystate, i, d & 1) ;
            }
            if (got(foo, wantup, wantready, or))
            {
              ftrigr_ack(&foo->a, foo->ids[i]) ;
              goto gotit ;
            }
          }
          ftrigr_ack(&foo->a, foo->ids[i]) ;
        }
      }
    }
  }
 gotit:
  return e ;
}
