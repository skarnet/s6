/* ISC license. */

#include <skalibs/skamisc.h>

#include <s6/servicedir.h>

int s6_servicedir_instances_recreate_offline (char const *old, char const *new)
{
  return s6_servicedir_instances_recreate_offline_tmp(old, new, &satmp) ;
}
