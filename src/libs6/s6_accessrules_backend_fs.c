/* ISC license. */

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <skalibs/bytestr.h>
#include <skalibs/fmtscan.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <s6/accessrules.h>

s6_accessrules_result_t s6_accessrules_backend_fs (char const *key, size_t keylen, void *data, s6_accessrules_params_t *params)
{
  char *dir = data ;
  size_t dirlen = str_len(dir) ;
  size_t envbase = params->env.len ;
  int wasnull = !params->env.s ;
  {
    char tmp[dirlen + keylen + 10] ;
    byte_copy(tmp, dirlen, dir) ;
    tmp[dirlen] = '/' ;
    byte_copy(tmp + dirlen + 1, keylen, key) ;
    byte_copy(tmp + dirlen + keylen + 1, 7, "/allow") ;
    if (access(tmp, R_OK) < 0)
    {
      if ((errno != EACCES) && (errno != ENOENT))
        return S6_ACCESSRULES_ERROR ;
      byte_copy(tmp + dirlen + keylen + 2, 5, "deny") ;
      return (access(tmp, R_OK) == 0) ? S6_ACCESSRULES_DENY :
       (errno != EACCES) && (errno != ENOENT) ? S6_ACCESSRULES_ERROR :
       S6_ACCESSRULES_NOTFOUND ;
    }
    byte_copy(tmp + dirlen + keylen + 2, 4, "env") ;
    if ((envdir(tmp, &params->env) < 0) && (errno != ENOENT))
      return S6_ACCESSRULES_ERROR ;
    if (!stralloc_readyplus(&params->exec, 4097))
    {
      if (wasnull) stralloc_free(&params->env) ;
      else params->env.len = envbase ;
      return S6_ACCESSRULES_ERROR ;
    }
    byte_copy(tmp + dirlen + keylen + 2, 5, "exec") ;
    {
      register ssize_t r = openreadnclose(tmp, params->exec.s + params->exec.len, 4096) ;
      if ((r < 0) && (errno != EACCES) && (errno != ENOENT))
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
