/* ISC license. */

#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <skalibs/bytestr.h>
#include <skalibs/uint16.h>
#include <skalibs/cdb.h>
#include <skalibs/stralloc.h>
#include <s6/accessrules.h>

s6_accessrules_result_t s6_accessrules_backend_cdb (char const *key, size_t keylen, void *data, s6_accessrules_params_t *params)
{
  struct cdb *c = data ;
  size_t execbase ;
  unsigned int n ;
  uint16_t envlen, execlen ;
  register int r = cdb_find(c, key, keylen) ;
  if (r < 0) return S6_ACCESSRULES_ERROR ;
  else if (!r) return S6_ACCESSRULES_NOTFOUND ;
  n = cdb_datalen(c) ;
  if ((n < 5U) || (n > 8197U)) return (errno = EINVAL, S6_ACCESSRULES_ERROR) ;
  if (!stralloc_readyplus(&params->exec, n)) return S6_ACCESSRULES_ERROR ;
  execbase = params->exec.len ;
  if (cdb_read(c, params->exec.s + execbase, n, cdb_datapos(c)) < 0) return S6_ACCESSRULES_ERROR ;
  if (params->exec.s[execbase] == 'D') return S6_ACCESSRULES_DENY ;
  else if (params->exec.s[execbase] != 'A') return S6_ACCESSRULES_NOTFOUND ;
  uint16_unpack_big(params->exec.s + execbase + 1U, &envlen) ;
  if ((envlen > 4096U) || (envlen+5U > n)) return (errno = EINVAL, S6_ACCESSRULES_ERROR) ;
  uint16_unpack_big(params->exec.s + execbase + 3 + envlen, &execlen) ;
  if ((execlen > 4096U) || (5U + envlen + execlen != n)) return (errno = EINVAL, S6_ACCESSRULES_ERROR) ;
  if (!stralloc_catb(&params->env, params->exec.s + execbase + 3U, envlen)) return S6_ACCESSRULES_ERROR ;
  byte_copy(params->exec.s + execbase, execlen, params->exec.s + execbase + 5U + envlen) ;
  if (execlen)
  {
    params->exec.len += execlen ;
    params->exec.s[params->exec.len++] = 0 ;
  }
  return S6_ACCESSRULES_ALLOW ;
}
