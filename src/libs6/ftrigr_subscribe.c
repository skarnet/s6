/* ISC license. */

#include <sys/types.h>
#include <stdint.h>
#include <errno.h>
#include <skalibs/uint16.h>
#include <skalibs/uint32.h>
#include <skalibs/siovec.h>
#include <skalibs/tai.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/skaclient.h>
#include <s6/ftrigr.h>

uint16_t ftrigr_subscribe (ftrigr_t *a, char const *path, char const *re, uint32_t options, tain_t const *deadline, tain_t *stamp)
{
  size_t pathlen = str_len(path) ;
  size_t relen = str_len(re) ;
  unsigned int i ;
  char err ;
  char tmp[15] = "--L" ;
  siovec_t v[3] = { { .s = tmp, .len = 15 }, { .s = (char *)path, .len = pathlen + 1 }, { .s = (char *)re, .len = relen + 1 } } ;
  if (!gensetdyn_new(&a->data, &i)) return 0 ;
  uint16_pack_big(tmp, (uint16_t)i) ;
  uint32_pack_big(tmp+3, options) ;
  uint32_pack_big(tmp+7, (uint32_t)pathlen) ;
  uint32_pack_big(tmp+11, (uint32_t)relen) ;
  if (!skaclient_sendv(&a->connection, v, 3, &skaclient_default_cb, &err, deadline, stamp))
  {
    register int e = errno ;
    gensetdyn_delete(&a->data, i) ;
    errno = e ;
    return 0 ;
  }
  if (err)
  {
    gensetdyn_delete(&a->data, i) ;
    return (errno = err, 0) ;
  }
  {
    register ftrigr1_t *p = GENSETDYN_P(ftrigr1_t, &a->data, i) ;
    p->options = options ;
    p->state = FR1STATE_LISTENING ;
    p->count = 0 ;
    p->what = 0 ;
  }
  return (uint16)(i+1) ;
}
