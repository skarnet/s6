/* ISC license. */

#include <skalibs/textclient.h>
#include <s6/ftrigr.h>

int ftrigr_start (ftrigr_t *a, char const *path, tain const *deadline, tain *stamp)
{
  return textclient_start(&a->connection, path, 0, FTRIGR_BANNER1, FTRIGR_BANNER1_LEN, FTRIGR_BANNER2, FTRIGR_BANNER2_LEN, deadline, stamp) ;
}
