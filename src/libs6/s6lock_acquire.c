/* ISC license. */

#include <sys/uio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <skalibs/uint16.h>
#include <skalibs/uint32.h>
#include <skalibs/tai.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/textclient.h>
#include <s6/lock.h>

int s6lock_acquire (s6lock_t *a, uint16_t *u, char const *path, uint32_t options, tain const *limit, tain const *deadline, tain *stamp)
{
  size_t pathlen = strlen(path) ;
  char tmp[23] = "--<" ;
  struct iovec v[2] = { { .iov_base = tmp, .iov_len = 23 }, { .iov_base = (char *)path, .iov_len = pathlen + 1 } } ;
  uint32_t i ;
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
  if (!textclient_commandv(&a->connection, v, 2, deadline, stamp))
  {
    int e = errno ;
    gensetdyn_delete(&a->data, i) ;
    errno = e ;
    return 0 ;
  }
  *GENSETDYN_P(unsigned char, &a->data, i) = EAGAIN ;
  *u = i ;
  return 1 ;
}
