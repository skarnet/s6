/* ISC license. */

#include <stdint.h>
#include <skalibs/genalloc.h>
#include <s6/ftrigr.h>

void ftrigr_ack (ftrigr_t *a, size_t n)
{
  size_t len = genalloc_len(uint16_t, &a->list) ;
  a->head += n ;
  if (a->head > len) a->head = len ;
  if (a->head == len)
  {
    a->head = 0 ;
    genalloc_setlen(uint16_t, &a->list, 0) ;
  }
}
