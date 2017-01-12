/* ISC license. */

#include <sys/types.h>
#include <skalibs/uint.h>
#include <skalibs/bytestr.h>
#include <skalibs/bitarray.h>
#include <skalibs/fmtscan.h>
#include <s6/accessrules.h>

s6_accessrules_result_t s6_accessrules_keycheck_ip6 (void const *key, void *data, s6_accessrules_params_t *params, s6_accessrules_backend_func_t_ref check1)
{
  char fmt[IP6_FMT + UINT_FMT + 6] = "ip6/" ;
  char ip6[16] ;
  unsigned int i = 0 ;
  byte_copy(ip6, 16, (char const *)key) ;
  for (; i <= 128 ; i++)
  {
    size_t len ;
    register s6_accessrules_result_t r ;
    if (i) bitarray_clear(ip6, 128 - i) ;
    len = 4 + ip6_fmt(fmt+4, ip6) ;
    fmt[len++] = '_' ;
    len += uint_fmt(fmt + len, 128 - i) ;
    r = (*check1)(fmt, len, data, params) ;
    if (r != S6_ACCESSRULES_NOTFOUND) return r ;
  }
  return S6_ACCESSRULES_NOTFOUND ;
}
