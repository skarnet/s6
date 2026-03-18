/* ISC license. */

#include <errno.h>

#include <skalibs/sassclient.h>

#include <s6/ftrigr.h>
#include "ftrigr-internal.h"

int ftrigr_unsubscribe (ftrigr *a, uint32_t id, tain const *deadline, tain *stamp)
{
  ftrigr_data *p ;
  if (!sassclient_cancel(&a->client, id, deadline, stamp)) return 0 ;
  p = genalloc_s(ftrigr_data, &a->data) + id ;
  p->sa.len = 0 ;
  p->status = EINVAL ;
  return 1 ;
}
