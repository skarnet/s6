/* ISC license. */

#include <skalibs/skamisc.h>

#include <s6/auto.h>

void s6_auto_write_logger (char const *dir, char const *loguser, char const *logdir, unsigned int stamptype, unsigned int nfiles, uint64_t filesize, uint64_t maxsize, char const *prefix, char const *service, char const *pipelinename)
{
  return s6_auto_write_logger_tmp(dir, loguser, logdir, stamptype, nfiles, filesize, maxsize, prefix, service, pipelinename, &satmp) ;
}
