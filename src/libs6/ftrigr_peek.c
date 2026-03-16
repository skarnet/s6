/* ISC license. */

#include <sys/uio.h>
#include <errno.h>

#include <skalibs/genalloc.h>

#include <s6/ftrigr.h>
#include "ftrigr-internal.h"

int ftrigr_peek (ftrigr *a, uint32_t id, struct iovec *v)
{
  ftrigr_data *p = genalloc_s(ftrigr_data, &a->data) + id ;
  switch (p->status)
  {
    case EAGAIN : return 0 ;
    case 0 :
    {
      v->iov_base = p->sa.s ;
      v->iov_len = p->sa.len ;
      return 1 ;
    }
    default: break ;
  }
  return (errno = p->status, -1) ;
}
