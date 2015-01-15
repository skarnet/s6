/* ISC license. */

#include <skalibs/diuint.h>
#include <skalibs/cdb.h>
#include <s6/accessrules.h>

s6_accessrules_result_t s6_accessrules_uidgid_cdb (unsigned int uid, unsigned int gid, struct cdb *c, s6_accessrules_params_t *params)
{
  diuint uidgid = { .left = uid, .right = gid } ;
  return s6_accessrules_keycheck_uidgid(&uidgid, c, params, &s6_accessrules_backend_cdb) ;
}
