/* ISC license. */

#include <errno.h>
#include <skalibs/environ.h>
#include <skalibs/tai.h>
#include <skalibs/skaclient.h>
#include <s6/s6lock.h>

int s6lock_start (s6lock_t *a, char const *path, tain_t const *deadline, tain_t *stamp)
{
  return skaclient_start_b(&a->connection, &a->buffers, path, S6LOCK_BANNER1, S6LOCK_BANNER1_LEN, S6LOCK_BANNER2, S6LOCK_BANNER2_LEN, deadline, stamp) ;
}
