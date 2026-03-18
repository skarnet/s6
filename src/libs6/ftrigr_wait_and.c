/* ISC license. */

#include <sys/uio.h>
#include <errno.h>

#include <skalibs/iopause.h>

#include <s6/ftrigr.h>

int ftrigr_wait_and (ftrigr *a, uint32_t const *list, unsigned int n, tain const *deadline, tain *stamp)
{
  iopause_fd x = { .fd = ftrigr_fd(a), .events = IOPAUSE_READ } ;
  for (unsigned int i = 0 ; i < n ; i++)
  {
    for (;;)
    {
      ftrigr_string fs ;
      int r = ftrigr_peek(a, list[i], &fs) ;
      if (r == -1) return -1 ;
      if (r) break ;
      r = iopause_stamp(&x, 1, deadline, stamp) ;
      if (r == -1) return -1 ;
      if (!r) return (errno = ETIMEDOUT, -1) ;
      if (ftrigr_update(a) == -1) return -1 ;
    }
    ftrigr_ack(a, list[i]) ;
  }
  return 0 ;
}
