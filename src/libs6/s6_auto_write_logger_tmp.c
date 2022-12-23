/* ISC license. */

#include <unistd.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/stat.h>

#include <skalibs/uint64.h>
#include <skalibs/strerr.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <s6/auto.h>

void s6_auto_write_logger_tmp (char const *dir, char const *loguser, char const *logdir, unsigned int stamptype, unsigned int nfiles, uint64_t filesize, uint64_t maxsize, char const *prefix, char const *service, char const *pipelinename, stralloc *sa)
{
  mode_t m = umask(0) ;
  size_t dirlen = strlen(dir) ;
  char fn[dirlen + 17] ;
  memcpy(fn, dir, dirlen) ;
  memcpy(fn + dirlen, "/notification-fd", 17) ;
  if (mkdir(dir, 0755) == -1) strerr_diefu2sys(111, "mkdir ", dir) ;
  umask(m) ;
  m = ~m & 0666 ;
  if (!openwritenclose_unsafe(fn, "3\n", 2)) goto err ;
  memcpy(fn + dirlen + 1, "run", 4) ;
  if (!s6_auto_write_logrun_tmp(fn, loguser, logdir, stamptype, nfiles, filesize, maxsize, prefix, sa)) goto err ;
  if (service)
  {
    struct iovec v[2] = { { .iov_base = (char *)service, .iov_len = strlen(service) }, { .iov_base = "\n", .iov_len = 1 } } ;
    memcpy(fn + dirlen + 1, "type", 5) ;
    if (!openwritenclose_unsafe(fn, "longrun\n", 8)) goto err ;
    memcpy(fn + dirlen + 1, "consumer-for", 13) ;
    if (!openwritevnclose_unsafe(fn, v, 2)) goto err ;
    if (pipelinename)
    {
      v[0].iov_base = (char *)pipelinename ;
      v[0].iov_len = strlen(pipelinename) ;
      memcpy(fn + dirlen + 1, "pipeline-name", 14) ;
      if (!openwritevnclose_unsafe(fn, v, 2)) goto err ;
    }
  }
  else
  {
    if (chmod(fn, m | ((m >> 2) & 0111)) == -1)
      strerr_diefu2sys(111, "chmod ", fn) ;
    if (!(m & 0400))
      strerr_warnw2x("weird umask, check permissions manually on ", fn) ;
  }
  return ;
 err:
  strerr_diefu2sys(111, "write to ", fn) ;
}
