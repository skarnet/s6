/* ISC license. */

#include <errno.h>
#include <signal.h>
#include <skalibs/sig.h>
#include <s6/ftrigw.h>

int ftrigw_notifyb (char const *path, char const *s, size_t len)
{
  struct skasigaction old ;
  int r ;
  if (skasigaction(SIGPIPE, &SKASIG_IGN, &old) < 0) return -1 ;
  r = ftrigw_notifyb_nosig(path, s, len) ;
  {
    int e = errno ;
    skasigaction(SIGPIPE, &old, 0) ;
    errno = e ;
  }
  return r ;
}
