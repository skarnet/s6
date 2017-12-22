/* ISC license. */

#include <skalibs/textclient.h>
#include <s6/s6lock.h>

int s6lock_start (s6lock_t *a, char const *path, tain_t const *deadline, tain_t *stamp)
{
  return textclient_start(&a->connection, path, 0, S6LOCK_BANNER1, S6LOCK_BANNER1_LEN, S6LOCK_BANNER2, S6LOCK_BANNER2_LEN, deadline, stamp) ;
}
