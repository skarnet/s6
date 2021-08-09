/* ISC license. */

#include <sys/uio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <skalibs/uint16.h>
#include <skalibs/uint32.h>
#include <skalibs/tai.h>
#include <skalibs/stralloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/textclient.h>
#include <s6/ftrigr.h>

uint16_t ftrigr_subscribe (ftrigr_t *a, char const *path, char const *re, uint32_t options, tain const *deadline, tain *stamp)
{
  size_t pathlen = strlen(path) ;
  size_t relen = strlen(re) ;
  ftrigr1_t ft = { .options = options, .state = FR1STATE_LISTENING, .what = STRALLOC_ZERO } ;
  uint32_t i ;
  char tmp[15] = "--L" ;
  struct iovec v[3] = { { .iov_base = tmp, .iov_len = 15 }, { .iov_base = (char *)path, .iov_len = pathlen + 1 }, { .iov_base = (char *)re, .iov_len = relen + 1 } } ;
  if (!gensetdyn_new(&a->data, &i)) return 0 ;
  if (i >= UINT16_MAX)
  {
    gensetdyn_delete(&a->data, i) ;
    return (errno = EMFILE, 0) ;
  }
  uint16_pack_big(tmp, (uint16_t)i) ;
  uint32_pack_big(tmp+3, options) ;
  uint32_pack_big(tmp+7, (uint32_t)pathlen) ;
  uint32_pack_big(tmp+11, (uint32_t)relen) ;
  if (!textclient_commandv(&a->connection, v, 3, deadline, stamp))
  {
    int e = errno ;
    gensetdyn_delete(&a->data, i) ;
    errno = e ;
    return 0 ;
  }
  *GENSETDYN_P(ftrigr1_t, &a->data, i) = ft ;
  return (uint16_t)(i+1) ;
}
