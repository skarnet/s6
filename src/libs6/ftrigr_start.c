/* ISC license. */

#include <skalibs/tai.h>
#include <skalibs/skaclient.h>
#include <s6/ftrigr.h>

int ftrigr_start (ftrigr_t *a, char const *path, tain_t const *deadline, tain_t *stamp)
{
  return skaclient_start_b(&a->connection, &a->buffers, path, 0, FTRIGR_BANNER1, FTRIGR_BANNER1_LEN, FTRIGR_BANNER2, FTRIGR_BANNER2_LEN, deadline, stamp) ;
}
