/* ISC license. */

#include <errno.h>
#include <signal.h>

#include <s6/ftrigw.h>

int ftrigw_notifyb (char const *path, char const *s, size_t len)
{
  struct sigaction old ;
  struct sigaction action = { .sa_handler = SIG_IGN, .sa_flags = SA_RESTART | SA_NOCLDSTOP } ;
  int r ;
  sigfillset(&action.sa_mask) ;
  if (sigaction(SIGPIPE, &action, &old) == -1) return -1 ;
  r = ftrigw_notifyb_nosig(path, s, len) ;
  {
    int e = errno ;
    sigaction(SIGPIPE, &old, 0) ;
    errno = e ;
  }
  return r ;
}
