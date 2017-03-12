/* ISC license. */

#include <sys/uio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <skalibs/types.h>
#include <skalibs/tai.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/skaclient.h>
#include <s6/s6lock.h>

int s6lock_acquire (s6lock_t *a, uint16_t *u, char const *path, uint32_t options, tain_t const *limit, tain_t const *deadline, tain_t *stamp)
{
  size_t pathlen = strlen(path) ;
  char err ;
  char tmp[23] = "--<" ;
  struct iovec v[2] = { { .iov_base = tmp, .iov_len = 23 }, { .iov_base = (char *)path, .iov_len = pathlen + 1 } } ;
  unsigned int i ;
  if (pathlen > UINT32_MAX) return (errno = ENAMETOOLONG, 0) ;
  if (!gensetdyn_new(&a->data, &i)) return 0 ;
  if (i > UINT16_MAX)
  {
    gensetdyn_delete(&a->data, i) ;
    return (errno = EMFILE, 0) ;
  }
  uint16_pack_big(tmp, (uint16_t)i) ;
  uint32_pack_big(tmp+3, options) ;
  tain_pack(tmp+7, limit) ;
  uint32_pack_big(tmp+19, (uint32_t)pathlen) ;
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
