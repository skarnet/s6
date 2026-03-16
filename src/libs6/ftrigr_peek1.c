/* ISC license. */

#include <sys/uio.h>

#include <s6/ftrigr.h>

int ftrigr_peek1 (ftrigr *a, uint32_t id, char *c)
{
  struct iovec v ;
  int r = ftrigr_peek(a, id, &v) ;
  if (r <= 0) return r ;
  *c = ((char *)v.iov_base)[v.iov_len - 1] ;
  return 1 ;
}
