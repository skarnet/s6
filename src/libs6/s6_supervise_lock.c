/* ISC license. */

#include <sys/types.h>
#include <sys/stat.h>
#include <s6/s6-supervise.h>

int s6_supervise_lock (char const *subdir)
{
  return s6_supervise_lock_mode(subdir, S_IRWXU, S_IRUSR | S_IWUSR) ;
}
