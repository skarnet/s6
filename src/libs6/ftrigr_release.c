/* ISC license. */

#include <stdint.h>
#include <errno.h>

#include <skalibs/genalloc.h>
#include <skalibs/sassclient.h>

#include <s6/ftrigr.h>
#include "ftrigr-internal.h"

void ftrigr_release (ftrigr *a, uint32_t id)
{
  ftrigr_data *p = genalloc_s(ftrigr_data, &a->data) + id ;
  sassclient_release(&a->client, id) ;
  p->status = EINVAL ;
  p->sa.len = 0 ;
}
