/* ISC license. */

#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <skalibs/uint16.h>
#include <skalibs/cdb.h>
#include <skalibs/stralloc.h>

#include <s6/accessrules.h>

s6_accessrules_result_t s6_accessrules_backend_cdb (char const *key, size_t keylen, void const *arg, s6_accessrules_params_t *params)
{
  cdb_data data ;
  int wasnull = !params->env.s ;
  uint16_t envlen, execlen ;
  cdb const *c = arg ;
  int r = cdb_find(c, &data, key, keylen) ;
  if (r < 0) return S6_ACCESSRULES_ERROR ;
  else if (!r) return S6_ACCESSRULES_NOTFOUND ;
  if (!data.len || data.len > 8197) return (errno = EINVAL, S6_ACCESSRULES_ERROR) ;
  if (data.s[0] == 'D') return S6_ACCESSRULES_DENY ;
  if (data.s[0] != 'A') return S6_ACCESSRULES_NOTFOUND ;
  if (data.len < 5) return (errno = EINVAL, S6_ACCESSRULES_ERROR) ;
  uint16_unpack_big(data.s + 1, &envlen) ;
  if ((envlen > 4096) || (data.len - 5 < envlen)) return (errno = EINVAL, S6_ACCESSRULES_ERROR) ;
  uint16_unpack_big(data.s + 3 + envlen, &execlen) ;
  if ((execlen > 4096) || (5 + envlen + execlen != data.len)) return (errno = EINVAL, S6_ACCESSRULES_ERROR) ;
  if (envlen && !stralloc_catb(&params->env, data.s + 3, envlen)) return S6_ACCESSRULES_ERROR ;
  if (execlen)
  {
    if (!stralloc_readyplus(&params->exec, execlen + 1))
    {
      if (envlen)
      {
        if (wasnull) stralloc_free(&params->env) ;
        else params->env.len -= envlen ;
      }
      return S6_ACCESSRULES_ERROR ;
    }
    stralloc_catb(&params->exec, data.s + 5 + envlen, execlen) ;
    stralloc_0(&params->exec) ;
  }
  return S6_ACCESSRULES_ALLOW ;
}
