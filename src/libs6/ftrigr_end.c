/* ISC license. */

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/sassclient.h>

#include <s6/ftrigr.h>
#include "ftrigr-internal.h"

static void ftrigr_data_free (void *x)
{
  ftrigr_data *p = x ;
  stralloc_free(&p->sa) ;
}

void ftrigr_end (ftrigr *a)
{
  sassclient_end(&a->client) ;
  a->data.len = a->data.a ;
  genalloc_deepfree(ftrigr_data, &a->data, &ftrigr_data_free) ;
}
