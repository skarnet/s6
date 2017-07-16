/* ISC license. */

#include <stdint.h>
#include <skalibs/genalloc.h>
#include <s6/ftrigr.h>

int ftrigr_update (ftrigr_t *a)
{
  genalloc_setlen(uint16_t, &a->list, 0) ;
  return ftrigr_updateb(a) ;
}
