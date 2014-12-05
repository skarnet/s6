/* ISC license. */

#include <errno.h>
#include <skalibs/uint16.h>
#include <skalibs/uint32.h>
#include <skalibs/bytestr.h>
#include <skalibs/siovec.h>
#include <skalibs/tai.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/skaclient.h>
#include <s6/s6lock.h>

int s6lock_acquire (s6lock_t *a, uint16 *u, char const *path, uint32 options, tain_t const *limit, tain_t const *deadline, tain_t *stamp)
{
  unsigned int pathlen = str_len(path) ;
  char err ;
  char tmp[23] = "--<" ;
  siovec_t v[2] = { { .s = tmp, .len = 23 }, { .s = (char *)path, .len = pathlen + 1 } } ;
  unsigned int i ;
  if (!gensetdyn_new(&a->data, &i)) return 0 ;
  uint16_pack_big(tmp, (uint16)i) ;
  uint32_pack_big(tmp+3, options) ;
  tain_pack(tmp+7, limit) ;
  uint32_pack_big(tmp+19, (uint32)pathlen) ;
  if (!skaclient_sendv(&a->connection, v, 2, &skaclient_default_cb, &err, deadline, stamp))
  {
    gensetdyn_delete(&a->data, i) ;
    return 0 ;
  }
  if (err)
  {
    gensetdyn_delete(&a->data, i) ;
    return (errno = err, 0) ;
  }
  *GENSETDYN_P(char, &a->data, i) = EAGAIN ;
  *u = i ;
  return 1 ;
}
