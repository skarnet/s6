/* ISC license. */

#include <skalibs/sassclient.h>

#include <s6/config.h>
#include <s6/ftrigr.h>
#include "ftrigr-internal.h"

int ftrigr_startf (ftrigr *a, tain const *deadline, tain *stamp)
{
  char const *argv[2] = { S6_LIBEXECPREFIX "s6-ftrigrd", 0 } ;
  return sassclient_start(&a->client, argv, FTRIGR_BANNER1, FTRIGR_BANNER2, deadline, stamp) ;
}
