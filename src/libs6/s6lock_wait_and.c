/* ISC license. */

#include <errno.h>
#include <skalibs/uint16.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <s6/s6lock.h>

int s6lock_wait_and (s6lock_t *a, uint16 const *idlist, unsigned int n, tain_t const *deadline, tain_t *stamp)
{
  iopause_fd x = { -1, IOPAUSE_READ, 0 } ;
  x.fd = s6lock_fd(a) ;
  for (; n ; n--, idlist++)
  {
    for (;;)
    {
      register int r = s6lock_check(a, *idlist) ;
      if (r < 0) return r ;
      else if (r) break ;
      r = iopause_stamp(&x, 1, deadline, stamp) ;
      if (r < 0) return r ;
      else if (!r) return (errno = ETIMEDOUT, -1) ;
      else if (s6lock_update(a) < 0) return -1 ;
    }
  }
  return 0 ;
}
