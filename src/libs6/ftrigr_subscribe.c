/* ISC license. */

#include <sys/uio.h>
#include <string.h>
#include <errno.h>

#include <s6/ftrigr.h>
#include "ftrigr-internal.h"

static int ftrigr_cb (char const *s, size_t len, uint32_t id, void *x)
{
  ftrigr_data *p = genalloc_s(ftrigr_data, (genalloc *)x) + id ;
  if (!stralloc_catb(&p->sa, s, len)) return errno ;
  p->status = 0 ;
  return 0 ;
}

int ftrigr_subscribe (ftrigr *a, uint32_t *i, uint32_t flags, uint32_t timeout, char const *path, char const *re, tain const *deadline, tain *stamp)
{
  ftrigr_data *p ;
  uint32_t id ;
  size_t pathlen = strlen(path) ;
  size_t relen = strlen(re) ;

  struct iovec v[2] =
  {
    { .iov_base = (char *)path, .iov_len = pathlen + 1 },
    { .iov_base = (char *)re, .iov_len = relen + 1 }
  } ;
  if (!sassclient_sendv(&a->client, &id, flags, timeout, 0, v, 2, &ftrigr_cb, &a->data, deadline, stamp)) return 0 ;
  if (!genalloc_ready(ftrigr_data, &a->data, id + 1))
  {
    int e = errno ;
    sassclient_cancel(&a->client, id, deadline, stamp) ;
    errno = e ;
    return 0 ;
  }
  p = genalloc_s(ftrigr_data, &a->data) + id ;
  p->sa.len = 0 ;
  p->status = EAGAIN ;
  *i = id ;
  return 1 ;
}
