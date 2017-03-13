/* ISC license. */

#include <s6/accessrules.h>

s6_accessrules_result_t s6_accessrules_uidgid_fs (uid_t uid, gid_t gid, char const *rulesdir, s6_accessrules_params_t *params)
{
  uidgid_t uidgid = { .left = uid, .right = gid } ;
  return s6_accessrules_keycheck_uidgid(&uidgid, (void *)rulesdir, params, &s6_accessrules_backend_fs) ;
}
