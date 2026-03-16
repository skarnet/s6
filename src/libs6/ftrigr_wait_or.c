/* ISC license. */

#include <sys/uio.h>
#include <errno.h>

#include <skalibs/iopause.h>

#include <s6/ftrigr.h>

int ftrigr_wait_or (ftrigr *a, uint32_t const *list, unsigned int n, struct iovec *v, tain const *deadline, tain *stamp)
{
  iopause_fd x = { .fd = ftrigr_fd(a), .events = IOPAUSE_READ } ;
  for (;;)
  {
    int r ;
    for (unsigned int i = 0 ; i < n ; i++)
    {
      r = ftrigr_peek(a, list[i], v) ;
      if (r == -1) return -1 ;
      if (r) return i ;
    }
    r = iopause_stamp(&x, 1, deadline, stamp) ;
    if (r == -1) return -1 ;
    if (!r) return (errno = ETIMEDOUT, -1) ;
    if (ftrigr_update(a) == -1) return -1 ;
  }
}
