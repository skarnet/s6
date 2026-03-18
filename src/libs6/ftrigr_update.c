/* ISC license. */

#include <stdint.h>

#include <skalibs/genalloc.h>
#include <skalibs/sassclient.h>

#include <s6/ftrigr.h>
#include "ftrigr-internal.h"

int ftrigr_update (ftrigr *a)
{
  int r = sassclient_update(&a->client) ;
  if (r <= 0) return r ;
  for (;;)
  {
    uint32_t id ;
    int status ;
    r = sassclient_ack(&a->client, &id, &status) ;
    if (r == -1) return -1 ;
    if (!r) break ;
    genalloc_s(ftrigr_data, &a->data)[id].status = status ;
  }
  return 1 ;
}
