/* ISC license. */

#include <errno.h>
#include <skalibs/bytestr.h>
#include <s6/accessrules.h>

s6_accessrules_result_t s6_accessrules_keycheck_reversedns (void const *key, void *data, s6_accessrules_params_t *params, s6_accessrules_backend_func_t_ref check1)
{
  char const *name = key ;
  unsigned int len = str_len(name) ;
  if (!len) return (errno = EINVAL, S6_ACCESSRULES_ERROR) ;
  if (name[len-1] == '.') len-- ;
  {
    unsigned int i = 0 ;
    char tmp[len + 11] ;
    byte_copy(tmp, 11, "reversedns/") ;
    while (i < len)
    {
      register s6_accessrules_result_t r ;
      byte_copy(tmp+11, len-i, name+i) ;
      r = (*check1)(tmp, 11+len-i, data, params) ;
      if (r != S6_ACCESSRULES_NOTFOUND) return r ;
      i += byte_chr(name+i, len-i, '.') + 1 ;
    }
  }
  return (*check1)("reversedns/@", 12, data, params) ;
}
