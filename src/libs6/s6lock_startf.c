/* ISC license. */

#include <errno.h>
#include <skalibs/posixplz.h>
#include <skalibs/textclient.h>
#include <s6/s6lock.h>

int s6lock_startf (s6lock_t *a, char const *lockdir, tain const *deadline, tain *stamp)
{
  char const *cargv[3] = { S6LOCKD_PROG, lockdir, 0 } ;
  if (!lockdir) return (errno = EINVAL, 0) ;
  return textclient_startf(&a->connection, cargv, (char const *const *)environ, TEXTCLIENT_OPTION_WAITPID, S6LOCK_BANNER1, S6LOCK_BANNER1_LEN, S6LOCK_BANNER2, S6LOCK_BANNER2_LEN, deadline, stamp) ;
}
