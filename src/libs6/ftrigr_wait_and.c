/* ISC license. */

#include <errno.h>
#include <skalibs/iopause.h>
#include <s6/ftrigr.h>

int ftrigr_wait_and (ftrigr_t *a, uint16_t const *idlist, unsigned int n, tain const *deadline, tain *stamp)
{
  iopause_fd x = { -1, IOPAUSE_READ, 0 } ;
  x.fd = ftrigr_fd(a) ;
  for (; n ; n--, idlist++)
  {
    for (;;)
    {
      char dummy ;
      int r = ftrigr_check(a, *idlist, &dummy) ;
      if (r < 0) return r ;
      else if (r) break ;
      r = iopause_stamp(&x, 1, deadline, stamp) ;
      if (r < 0) return r ;
      else if (!r) return (errno = ETIMEDOUT, -1) ;
      else if (ftrigr_updateb(a) < 0) return -1 ;
    }
  }

  return 1 ;
}
