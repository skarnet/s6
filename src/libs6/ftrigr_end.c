/* ISC license. */

#include <stdint.h>
#include <skalibs/genalloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/textclient.h>
#include <s6/ftrigr.h>

void ftrigr_end (ftrigr_ref a)
{
  gensetdyn_free(&a->data) ;
  genalloc_free(uint16_t, &a->list) ;
  textclient_end(&a->connection) ;
  *a = ftrigr_zero ;
}
