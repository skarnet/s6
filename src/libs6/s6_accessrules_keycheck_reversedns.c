/* ISC license. */

#include <string.h>
#include <errno.h>
#include <skalibs/bytestr.h>
#include <s6/accessrules.h>

s6_accessrules_result_t s6_accessrules_keycheck_reversedns (void const *key, void const *data, s6_accessrules_params_t *params, s6_accessrules_backend_func_ref check1)
{
  char const *name = key ;
  size_t len = strlen(name) ;
  if (!len) return (errno = EINVAL, S6_ACCESSRULES_ERROR) ;
  if (name[len-1] == '.') len-- ;
  {
    size_t i = 0 ;
    char tmp[len + 11] ;
    memcpy(tmp, "reversedns/", 11) ;
    while (i < len)
    {
      s6_accessrules_result_t r ;
      memcpy(tmp+11, name+i, len-i) ;
      r = (*check1)(tmp, 11+len-i, data, params) ;
      if (r != S6_ACCESSRULES_NOTFOUND) return r ;
      i += byte_chr(name+i, len-i, '.') + 1 ;
    }
  }
  return (*check1)("reversedns/@", 12, data, params) ;
}
