/* ISC license. */

#include <s6/accessrules.h>

s6_accessrules_result_t s6_accessrules_uidgid_cdb (uid_t uid, gid_t gid, cdb const *c, s6_accessrules_params_t *params)
{
  uidgid_t uidgid = { .left = uid, .right = gid } ;
  return s6_accessrules_keycheck_uidgid(&uidgid, c, params, &s6_accessrules_backend_cdb) ;
}
