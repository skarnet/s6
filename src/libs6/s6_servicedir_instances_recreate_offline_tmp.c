/* ISC license. */

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <s6/servicedir.h>

int s6_servicedir_instances_recreate_offline_tmp (char const *old, char const *new, stralloc *sa)
{
  int n ;
  mode_t m ;
  size_t maxlen = 0 ;
  size_t sabase = sa->len ;
  size_t oldlen = strlen(old) ;
  size_t newlen = strlen(new) ;

  {
    char olddir[oldlen + 11] ;
    memcpy(olddir, old, oldlen) ;
    memcpy(olddir + oldlen, "/instances", 11) ;
    n = sals(olddir, sa, &maxlen) ;
  }
  if (n == -1) return errno == ENOENT ? 0 : -1 ;

  {
    size_t pos = sabase ;
    char olddir[oldlen + 17 + maxlen] ;
    char templatedir[newlen + 10] ;
    char newsd[newlen + 11 + maxlen] ;
    char newdir[newlen + 17 + maxlen] ;
    char lnk[14 + maxlen] ;
    memcpy(olddir, old, oldlen) ;
    memcpy(olddir + oldlen, "/instances/", 11) ;
    memcpy(templatedir, new, newlen) ;
    memcpy(templatedir + newlen, "/template", 10) ;
    memcpy(newsd, new, newlen) ;
    memcpy(newsd + newlen, "/instance", 10) ;
    memcpy(newdir, newsd, newlen + 9) ;
    newdir[newlen + 9] = 's' ; newdir[newlen + 10] = 0 ;
    memcpy(lnk, "../instances/", 13) ;
    m = umask(0) ;
    if (mkdir(newsd, 0755) == -1 && errno != EEXIST) goto merr ;
    if (mkdir(newdir, 0755) == -1 && errno != EEXIST) goto merr ;
    umask(m) ;
    newsd[newlen+9] = '/' ;
    newdir[newlen+10] = '/' ;
    while (pos < sa->len)
    {
      size_t len = strlen(sa->s + pos) ;
      memcpy(olddir + oldlen + 11, sa->s + pos, len) ;
      memcpy(olddir + oldlen + 11 + len, "/down", 6) ;
      memcpy(newsd + newlen + 10, sa->s + pos, len + 1) ;
      memcpy(newdir + newlen + 11, sa->s + pos, len + 1) ;
      memcpy(lnk + 13, sa->s + pos, len + 1) ;
      if (!hiercopy_tmp(templatedir, newdir, sa)) goto err ;
      if (symlink(lnk, newsd) == -1 && errno != EEXIST) goto err ;
      if (access(olddir, F_OK) == 0)
      {
        memcpy(newdir + newlen + 11 + len, "/down", 6) ;
        if (!openwritenclose_unsafe(newdir, "", 0)) goto err ;
      }
      else if (errno != ENOENT) goto err ;
      pos += len + 1 ;
    }
  }
  return n ;

 merr:
  umask(m) ;
 err:
  sa->len = sabase ;
  return -1 ;
}
