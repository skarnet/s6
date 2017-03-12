/* ISC license. */

#include <string.h>
#include <s6/s6-supervise.h>

int s6_svc_writectl (char const *service, char const *subdir, char const *s, size_t len)
{
  size_t svlen = strlen(service) ;
  size_t sublen = strlen(subdir) ;
  char fn[svlen + sublen + 10] ;
  memcpy(fn, service, svlen) ;
  fn[svlen] = '/' ;
  memcpy(fn + svlen + 1, subdir, sublen) ;
  memcpy(fn + svlen + 1 + sublen, "/control", 9) ;
  return s6_svc_write(fn, s, len) ;
}
