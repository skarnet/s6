/* ISC license. */

#include <unistd.h>

#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <s6/accessrules.h>

s6_accessrules_result_t s6_accessrules_keycheck_uidgid (void const *key, void const *data, s6_accessrules_params_t *params, s6_accessrules_backend_func_ref check1)
{
  uidgid_t const *uidgid = key ;
  char fmt[4 + UINT64_FMT] = "uid/" ;
  s6_accessrules_result_t r ;
  if (uidgid->left == geteuid())
  {
    r = (*check1)("uid/self", 8, data, params) ;
    if (r != S6_ACCESSRULES_NOTFOUND) return r ;
  }
  r = (*check1)(fmt, 4 + uid_fmt(fmt+4, uidgid->left), data, params) ;
  if (r != S6_ACCESSRULES_NOTFOUND) return r ;
  if (uidgid->right == getegid())
  {
    r = (*check1)("gid/self", 8, data, params) ;
    if (r != S6_ACCESSRULES_NOTFOUND) return r ;
  }
  fmt[0] = 'g' ;
  r = (*check1)(fmt, 4 + gid_fmt(fmt+4, uidgid->right), data, params) ;
  if (r != S6_ACCESSRULES_NOTFOUND) return r ;
  return (*check1)("uid/default", 11, data, params) ;
}
