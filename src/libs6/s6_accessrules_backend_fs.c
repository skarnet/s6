/* ISC license. */

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <skalibs/stralloc.h>
#include <skalibs/env.h>
#include <skalibs/djbunix.h>
#include <s6/accessrules.h>

s6_accessrules_result_t s6_accessrules_backend_fs (char const *key, size_t keylen, void const *data, s6_accessrules_params_t *params)
{
  char const *dir = data ;
  size_t dirlen = strlen(dir) ;
  size_t envbase = params->env.len ;
  int wasnull = !params->env.s ;
  {
    char tmp[dirlen + keylen + 10] ;
    memcpy(tmp, dir, dirlen) ;
    tmp[dirlen] = '/' ;
    memcpy(tmp + dirlen + 1, key, keylen) ;
    memcpy(tmp + dirlen + keylen + 1, "/allow", 7) ;
    if (access(tmp, R_OK) < 0)
    {
      if ((errno != EACCES) && (errno != ENOENT))
        return S6_ACCESSRULES_ERROR ;
      memcpy(tmp + dirlen + keylen + 2, "deny", 5) ;
      return (access(tmp, R_OK) == 0) ? S6_ACCESSRULES_DENY :
       (errno != EACCES) && (errno != ENOENT) ? S6_ACCESSRULES_ERROR :
       S6_ACCESSRULES_NOTFOUND ;
    }
    memcpy(tmp + dirlen + keylen + 2, "env", 4) ;
    if ((envdir(tmp, &params->env) < 0) && (errno != ENOENT))
      return S6_ACCESSRULES_ERROR ;
    if (!stralloc_readyplus(&params->exec, 4097))
    {
      if (wasnull) stralloc_free(&params->env) ;
      else params->env.len = envbase ;
      return S6_ACCESSRULES_ERROR ;
    }
    memcpy(tmp + dirlen + keylen + 2, "exec", 5) ;
    {
      ssize_t r = openreadnclose(tmp, params->exec.s + params->exec.len, 4096) ;
      if ((r == -1) && (errno != EACCES) && (errno != ENOENT))
      {
        if (wasnull) stralloc_free(&params->env) ;
        else params->env.len = envbase ;
        return S6_ACCESSRULES_ERROR ;
      }
      if (r > 0)
      {
        params->exec.len += r ;
        params->exec.s[params->exec.len++] = 0 ;
      }
    }
  }
  return S6_ACCESSRULES_ALLOW ;
}
