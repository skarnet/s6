/* ISC license. */

#include <skalibs/stralloc.h>
#include <s6/accessrules.h>

void s6_accessrules_params_free (s6_accessrules_params_t *params)
{
  stralloc_free(&params->env) ;
  stralloc_free(&params->exec) ;
}
