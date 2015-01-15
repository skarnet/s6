/* ISC license. */

#include <skalibs/diuint.h>
#include <s6/accessrules.h>

s6_accessrules_result_t s6_accessrules_uidgid_fs (unsigned int uid, unsigned int gid, char const *rulesdir, s6_accessrules_params_t *params)
{
  diuint uidgid = { .left = uid, .right = gid } ;
  return s6_accessrules_keycheck_uidgid(&uidgid, (void *)rulesdir, params, &s6_accessrules_backend_fs) ;
}
