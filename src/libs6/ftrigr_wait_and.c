/* ISC license. */

#include <errno.h>
#include <skalibs/uint16.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <s6/ftrigr.h>

int ftrigr_wait_and (ftrigr_t *a, uint16 const *idlist, unsigned int n, tain_t const *deadline, tain_t *stamp)
{
  iopause_fd x = { -1, IOPAUSE_READ, 0 } ;
  x.fd = ftrigr_fd(a) ;
  for (; n ; n--, idlist++)
  {
    for (;;)
    {
      char dummy ;
      register int r = ftrigr_check(a, *idlist, &dummy) ;
      if (r < 0) return r ;
      else if (r) break ;
      r = iopause_stamp(&x, 1, deadline, stamp) ;
      if (r < 0) return r ;
      else if (!r) return (errno = ETIMEDOUT, -1) ;
      else if (ftrigr_update(a) < 0) return -1 ;
    }
  }
  return 1 ;
}
