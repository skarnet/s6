/* ISC license. */

#include <errno.h>
#include <skalibs/error.h>
#include <skalibs/uint16.h>
#include <skalibs/gensetdyn.h>
#include <s6/s6lock.h>

int s6lock_check (s6lock_t *a, uint16 id)
{
  char *p = GENSETDYN_P(char, &a->data, id) ;
  switch (*p)
  {
    case EBUSY : return 1 ;
    case EINVAL : return (errno = EINVAL, -1) ;
    default :
    {
      if (error_isagain(*p)) return 0 ;
      errno = *p ;
      *p = EINVAL ;
      gensetdyn_delete(&a->data, id) ;
      return -1 ;
    }
  }
}
