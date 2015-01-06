/* ISC license. */

#include <skalibs/tai.h>
#include <skalibs/skaclient.h>
#include <s6/ftrigr.h>

int ftrigr_startf (ftrigr_t *a, tain_t const *deadline, tain_t *stamp)
{
  static char const *const cargv[2] = { FTRIGRD_PROG, 0 } ;
  static char const *const cenvp[1] = { 0 } ;
  return skaclient_startf_b(&a->connection, &a->buffers, cargv[0], cargv, cenvp, SKACLIENT_OPTION_WAITPID, FTRIGR_BANNER1, FTRIGR_BANNER1_LEN, FTRIGR_BANNER2, FTRIGR_BANNER2_LEN, deadline, stamp) ;
}
