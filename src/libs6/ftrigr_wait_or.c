/* ISC license. */

#include <errno.h>

#include <skalibs/iopause.h>

#include <s6/ftrigr.h>

#include <skalibs/posixishard.h>

int ftrigr_wait_or (ftrigr_t *a, uint16_t const *idlist, unsigned int n, tain const *deadline, tain *stamp, char *c)
{
  iopause_fd x = { -1, IOPAUSE_READ | IOPAUSE_EXCEPT, 0 } ;
  x.fd = ftrigr_fd(a) ;
  if (x.fd < 0) return -1 ;
  for (;;)
  {
    unsigned int i = 0 ;
    int r ;
    for (; i < n ; i++)
    {
      r = ftrigr_check(a, idlist[i], c) ;
      if (r < 0) return r ;
      else if (r) return i ;
    }
    r = iopause_stamp(&x, 1, deadline, stamp) ;
    if (r < 0) return 0 ;
    else if (!r) return (errno = ETIMEDOUT, -1) ;
    else if (ftrigr_update(a) < 0) return -1 ;
  }
  return (errno = EPROTO, -1) ; /* can't happen */
}
