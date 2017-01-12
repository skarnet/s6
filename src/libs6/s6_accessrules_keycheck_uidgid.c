/* ISC license. */

#include <skalibs/uint64.h>
#include <skalibs/gidstuff.h>
#include <s6/accessrules.h>

s6_accessrules_result_t s6_accessrules_keycheck_uidgid (void const *key, void *data, s6_accessrules_params_t *params, s6_accessrules_backend_func_t_ref check1)
{
  char fmt[4 + UINT64_FMT] = "uid/" ;
  register s6_accessrules_result_t r = (*check1)(fmt, 4 + uint64_fmt(fmt+4, ((uidgid_t const *)key)->left), data, params) ;
  if (r != S6_ACCESSRULES_NOTFOUND) return r ;
  fmt[0] = 'g' ;
  r = (*check1)(fmt, 4 + gid_fmt(fmt+4, ((uidgid_t const *)key)->right), data, params) ;
  return (r != S6_ACCESSRULES_NOTFOUND) ? r :
   (*check1)("uid/default", 11, data, params) ;
}
