/* ISC license. */

#include <errno.h>
#include <skalibs/environ.h>
#include <skalibs/tai.h>
#include <skalibs/skaclient.h>
#include <s6/s6lock.h>

int s6lock_startf (s6lock_t *a, char const *lockdir, tain_t const *deadline, tain_t *stamp)
{
  char const *cargv[3] = { S6LOCKD_PROG, lockdir, 0 } ;
  if (!lockdir) return (errno = EINVAL, 0) ;
  return skaclient_startf_b(&a->connection, &a->buffers, cargv[0], cargv, (char const *const *)environ, SKACLIENT_OPTION_WAITPID, S6LOCK_BANNER1, S6LOCK_BANNER1_LEN, S6LOCK_BANNER2, S6LOCK_BANNER2_LEN, deadline, stamp) ;
}
