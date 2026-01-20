/* ISC license. */

#include <skalibs/posixplz.h>
#include <skalibs/textclient.h>

#include <s6/ftrigr.h>

int ftrigr_startf (ftrigr_t *a, tain const *deadline, tain *stamp)
{
  static char const *const cargv[2] = { FTRIGRD_PROG, 0 } ;
  return textclient_startf(&a->connection, cargv, (char const *const *)environ, TEXTCLIENT_OPTION_WAITPID, FTRIGR_BANNER1, FTRIGR_BANNER1_LEN, FTRIGR_BANNER2, FTRIGR_BANNER2_LEN, deadline, stamp) ;
}
