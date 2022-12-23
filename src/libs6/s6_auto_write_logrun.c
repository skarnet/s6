/* ISC license. */

#include <skalibs/skamisc.h>

#include <s6/auto.h>

int s6_auto_write_logrun (char const *runfile, char const *loguser, char const *logdir, unsigned int stamptype, unsigned int nfiles, uint64_t filesize, uint64_t maxsize, char const *prefix)
{
  return s6_auto_write_logrun_tmp(runfile, loguser, logdir, stamptype, nfiles, filesize, maxsize, prefix, &satmp) ;
}
