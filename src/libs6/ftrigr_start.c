/* ISC license. */

#include <skalibs/tai.h>

#include <s6/ftrigr.h>

int ftrigr_start (ftrigr *a, unsigned int sec)
{
  tain deadline = TAIN_INFINITE ;
  if (!tain_now_set_stopwatch_g()) return 0 ;
  if (sec) tain_addsec_g(&deadline, sec) ;
  return ftrigr_startf_g(a, &deadline) ;
}
