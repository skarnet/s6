/* ISC license. */

#ifndef S6_ACCESSRULES_H
#define S6_ACCESSRULES_H

#include <skalibs/cdb.h>
#include <skalibs/stralloc.h>
#include <skalibs/ip46.h>

typedef struct s6_accessrules_params_s s6_accessrules_params_t, *s6_accessrules_params_t_ref ;
struct s6_accessrules_params_s
{
  stralloc env ;
  stralloc exec ;
} ;
#define S6_ACCESSRULES_PARAMS_ZERO { .env = STRALLOC_ZERO, .exec = STRALLOC_ZERO }

typedef enum s6_accessrules_result_e s6_accessrules_result_t, *s6_accessrules_result_t_ref ;
enum s6_accessrules_result_e
{
  S6_ACCESSRULES_ERROR = -1,
  S6_ACCESSRULES_DENY = 0,
  S6_ACCESSRULES_ALLOW = 1,
  S6_ACCESSRULES_NOTFOUND = 2
} ;

typedef s6_accessrules_result_t s6_accessrules_backend_func_t (char const *, unsigned int, void *, s6_accessrules_params_t *) ;
typedef s6_accessrules_backend_func_t *s6_accessrules_backend_func_t_ref ;

extern s6_accessrules_backend_func_t s6_accessrules_backend_fs ;
extern s6_accessrules_backend_func_t s6_accessrules_backend_cdb ;

typedef s6_accessrules_result_t s6_accessrules_keycheck_func_t (void const *, void *, s6_accessrules_params_t *, s6_accessrules_backend_func_t_ref) ;
typedef s6_accessrules_keycheck_func_t *s6_accessrules_keycheck_func_t_ref ;

extern s6_accessrules_keycheck_func_t s6_accessrules_keycheck_uidgid ;
extern s6_accessrules_keycheck_func_t s6_accessrules_keycheck_ip4 ;
extern s6_accessrules_keycheck_func_t s6_accessrules_keycheck_ip6 ;
extern s6_accessrules_keycheck_func_t s6_accessrules_keycheck_reversedns ;
#define s6_accessrules_keycheck_ip46(key, data, params, f) (ip46_is6((ip46_t const *)(key)) ? s6_accessrules_keycheck_ip6(((ip46_t const *)(key))->ip, data, params, f) : s6_accessrules_keycheck_ip4(((ip46_t const *)(key))->ip, data, params, f))

extern s6_accessrules_result_t s6_accessrules_uidgid_cdb (unsigned int, unsigned int, struct cdb *, s6_accessrules_params_t *) ;
extern s6_accessrules_result_t s6_accessrules_uidgid_fs (unsigned int, unsigned int, char const *, s6_accessrules_params_t *) ;
#define s6_accessrules_ip4_cdb(ip4, c, params) s6_accessrules_keycheck_ip4(ip4, c, (params), &s6_accessrules_backend_cdb)
#define s6_accessrules_ip4_fs(ip4, rulesdir, params) s6_accessrules_keycheck_ip4(ip4, rulesdir, (params), &s6_accessrules_backend_fs)
#define s6_accessrules_ip6_cdb(ip6, c, params) s6_accessrules_keycheck_ip6(ip6, c, (params), &s6_accessrules_backend_cdb)
#define s6_accessrules_ip6_fs(ip6, rulesdir, params) s6_accessrules_keycheck_ip6(ip6, rulesdir, (params), &s6_accessrules_backend_fs)
#define s6_accessrules_ip46_cdb(ip, c, params) s6_accessrules_keycheck_ip46(ip, c, (params), &s6_accessrules_backend_cdb)
#define s6_accessrules_ip46_fs(ip, rulesdir, params) s6_accessrules_keycheck_ip46(ip, rulesdir, (params), &s6_accessrules_backend_fs)
#define s6_accessrules_reversedns_cdb(name, c, params) s6_accessrules_keycheck_reversedns(name, c, (params), &s6_accessrules_backend_cdb)
#define s6_accessrules_reversedns_fs(name, c, params) s6_accessrules_keycheck_reversedns(name, c, (params), &s6_accessrules_backend_fs)

#endif
