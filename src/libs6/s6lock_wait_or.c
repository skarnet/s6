/* ISC license. */

#include <errno.h>
#include <skalibs/error.h>
#include <skalibs/iopause.h>
#include <s6/s6lock.h>

int s6lock_wait_or (s6lock_t *a, uint16_t const *idlist, unsigned int n, tain_t const *deadline, tain_t *stamp)
{
  iopause_fd x = { -1, IOPAUSE_READ | IOPAUSE_EXCEPT, 0 } ;
  x.fd = s6lock_fd(a) ;
  if (x.fd < 0) return -1 ;
  for (;;)
  {
    unsigned int i = 0 ;
    int r ;
    for (; i < n ; i++)
    {
      r = s6lock_check(a, idlist[i]) ;
      if (r < 0) return r ;
      else if (r) return i ;
    }
    r = iopause_stamp(&x, 1, deadline, stamp) ;
    if (r < 0) return 0 ;
    else if (!r) return (errno = ETIMEDOUT, -1) ;
    else if (s6lock_update(a) < 0) return -1 ;
  }
  return (errno = EPROTO, -1) ; /* can't happen */
}
