/* ISC license. */

#include <errno.h>

#include <skalibs/genalloc.h>

#include <s6/ftrigr.h>
#include "ftrigr-internal.h"

int ftrigr_peek (ftrigr *a, uint32_t id, ftrigr_string *fs)
{
  ftrigr_data *p = genalloc_s(ftrigr_data, &a->data) + id ;
  switch (p->status)
  {
    case EAGAIN : return 0 ;
    case 0 :
    {
      fs->s = p->sa.s ;
      fs->len = p->sa.len ;
      return 1 ;
    }
    default: break ;
  }
  return (errno = p->status, -1) ;
}
