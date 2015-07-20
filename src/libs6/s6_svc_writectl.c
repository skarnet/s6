/* ISC license. */

#include <skalibs/bytestr.h>
#include <s6/s6-supervise.h>

int s6_svc_writectl (char const *service, char const *subdir, char const *s, unsigned int len)
{
  unsigned int svlen = str_len(service) ;
  unsigned int sublen = str_len(subdir) ;
  char fn[svlen + sublen + 10] ;
  byte_copy(fn, svlen, service) ;
  fn[svlen] = '/' ;
  byte_copy(fn + svlen + 1, sublen, subdir) ;
  byte_copy(fn + svlen + 1 + sublen, 9, "/control") ;
  return s6_svc_write(fn, s, len) ;
}
