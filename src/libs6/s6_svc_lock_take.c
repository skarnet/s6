/* ISC license. */

#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <skalibs/djbunix.h>
#include <s6/s6-supervise.h>

 /* XXX: does not work with dangling S6_SUPERVISE_CTLDIR symlinks */

int s6_svc_lock_take (char const *dir)
{
  size_t dirlen = strlen(dir) ;
  int fdlock ;
  char lock[dirlen + sizeof(S6_SUPERVISE_CTLDIR) + 6] ;
  memcpy(lock, dir, dirlen) ;
  memcpy(lock + dirlen, "/" S6_SUPERVISE_CTLDIR, sizeof(S6_SUPERVISE_CTLDIR) + 1) ;
  if ((mkdir(lock, S_IRWXU) < 0) && (errno != EEXIST)) return -1 ;
  memcpy(lock + dirlen + sizeof(S6_SUPERVISE_CTLDIR), "/lock", 6) ;
  fdlock = openc_create(lock) ;
  if (fdlock < 0) return -1 ;
  if (lock_ex(fdlock) < 0)
  {
    fd_close(fdlock) ;
    return -1 ;
  }
  return fdlock ;
}
