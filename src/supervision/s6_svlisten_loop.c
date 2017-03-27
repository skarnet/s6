/* ISC license. */

#include <string.h>
#include <skalibs/bytestr.h>
#include <skalibs/bitarray.h>
#include <skalibs/strerr2.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <s6/ftrigr.h>
#include <s6/s6-supervise.h>
#include "s6-svlisten.h"

void s6_svlisten_init (int argc, char const *const *argv, s6_svlisten_t *foo, uint16_t *ids, unsigned char *upstate, unsigned char *readystate, tain_t const *deadline)
{
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
    foo->ids[i] = ftrigr_subscribe_g(&foo->a, s, "[DuUdO]", FTRIGR_REPEAT, deadline) ;
    if (!foo->ids[i]) strerr_diefu2sys(111, "subscribe to events for ", argv[i]) ;
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

int s6_svlisten_loop (s6_svlisten_t *foo, int wantup, int wantready, int or, tain_t const *deadline, int spfd, action_func_t_ref handler)
{
  iopause_fd x[2] = { { .fd = ftrigr_fd(&foo->a), .events = IOPAUSE_READ }, { .fd = spfd, .events = IOPAUSE_READ, .revents = 0 } } ;
  unsigned int e = 0 ;
  while (!got(foo, wantup, wantready, or))
  {
    int r = iopause_g(x, 1 + (spfd >= 0), deadline) ;
    if (r < 0) strerr_diefu1sys(111, "iopause") ;
    else if (!r) strerr_dief1x(99, "timed out") ;
    if (x[1].revents & IOPAUSE_READ) (*handler)() ;
    if (x[0].revents & IOPAUSE_READ)
    {
      unsigned int i = 0 ;
      if (ftrigr_update(&foo->a) < 0) strerr_diefu1sys(111, "ftrigr_update") ;
      for (; i < foo->n ; i++)
      {
        char what ;
        int r = ftrigr_check(&foo->a, foo->ids[i], &what) ;
        if (r < 0) strerr_diefu1sys(111, "ftrigr_check") ;
        if (r)
        {
          if (what == 'O')
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
            unsigned int d = byte_chr("dDuU", 4, what) ;
            bitarray_poke(foo->upstate, i, d & 2) ;
            bitarray_poke(foo->readystate, i, d & 1) ;
          }
        }
      }
    }
  }
  return e ;
}
