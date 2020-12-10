/* ISC license. */

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <sys/stat.h>

#include <skalibs/bitarray.h>
#include <skalibs/tai.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <s6/ftrigr.h>
#include <s6/ftrigw.h>
#include <s6/s6-supervise.h>

static inline void do_unlink (char const *scdir, char const *prefix, size_t prefixlen, size_t maxlen, char const *names, size_t nameslen, uint32_t killopts)
{
  char fn[prefixlen + maxlen + 1] ;
  memcpy(fn, prefix, prefixlen) ;
  while (nameslen)
  {
    size_t len = strlen(names) + 1 ;
    memcpy(fn + prefixlen, names, len) ;
    s6_supervise_unlink(scdir, fn, killopts) ;
    names += len ; nameslen -= len ;
  }
}

int s6_supervise_link (char const *scdir, char const *const *servicedirs, size_t n, char const *prefix, uint32_t options, tain_t const *deadline, tain_t *stamp)
{
  size_t maxlen = 0 ;
  size_t ntotal = n ;
  unsigned char locked[bitarray_div8(n)] ;
  unsigned char logged[bitarray_div8(n)] ;
  if (!n) return 0 ;
  memset(locked, 0, bitarray_div8(n)) ;
  memset(logged, 0, bitarray_div8(n)) ;

  for (size_t i = 0 ; i < n ; i++)
  {
    struct stat st ;
    size_t len = strlen(servicedirs[i]) ;
    int h ;
    char subdir[len + 5] ;
    if (len > maxlen) maxlen = len ;
    h = s6_svc_ok(servicedirs[i]) ;
    if (h < 0) return -1 ;
    if (h) bitarray_set(locked, i) ;
    memcpy(subdir, servicedirs[i], len) ;
    memcpy(subdir + len, "/log", 5) ;
    if (stat(subdir, &st) < 0)
    {
      if (errno != ENOENT) return -1 ;
    }
    else
    {
      int r ;
      if (!S_ISDIR(st.st_mode)) return (errno = ENOTDIR, -1) ;
      r = s6_svc_ok(subdir) ;
      if (r < 0) return -1 ;
      if (r != h) return (errno = EINVAL, -1) ;
      bitarray_set(logged, i) ;
      ntotal++ ;
    }
  } 

  {
    stralloc lnames = STRALLOC_ZERO ;
    ftrigr_t a = FTRIGR_ZERO ;
    stralloc rpsa = STRALLOC_ZERO ;
    gid_t gid = options & 2 ? -1 : getegid() ;
    size_t scdirlen = strlen(scdir) ;
    size_t prefixlen = strlen(prefix) ;
    unsigned int m = 0 ;
    size_t i = 0 ;
    size_t lstart = 0 ;
    uint32_t killopts = 0 ;
    int r ;
    uint16_t ids[ntotal] ;
    char lname[scdirlen + prefixlen + maxlen + 2] ;
    char fn[maxlen + 5 + sizeof(S6_SUPERVISE_EVENTDIR)] ;
    if (!ftrigr_startf(&a, deadline, stamp)) return -1 ;
    memcpy(lname, scdir, scdirlen) ;
    lname[scdirlen] = '/' ;
    memcpy(lname + scdirlen + 1, prefix, prefixlen) ;
    for (i = 0 ; i < n ; i++) if (!bitarray_peek(locked, i))
    {
      char *p ;
      size_t len = strlen(servicedirs[i]) ;
      memcpy(fn, servicedirs[i], len) ;
      fn[len] = '/' ;
      memcpy(fn + len + 1, S6_SUPERVISE_EVENTDIR, sizeof(S6_SUPERVISE_EVENTDIR)) ;
      if (!ftrigw_fifodir_make(fn, gid, options & 1)) goto err ;
      ids[m] = ftrigr_subscribe(&a, fn, "s", 0, deadline, stamp) ;
      if (!ids[m++]) goto err ;
      if (bitarray_peek(logged, i))
      {
        memcpy(fn + len + 1, "log/", 4) ;
        memcpy(fn + len + 5, S6_SUPERVISE_EVENTDIR, sizeof(S6_SUPERVISE_EVENTDIR)) ;
        if (!ftrigw_fifodir_make(fn, gid, options & 1)) goto err ;
        ids[m] = ftrigr_subscribe(&a, fn, "s", 0, deadline, stamp) ;
        if (!ids[m++]) goto err ;
      }
      fn[len] = 0 ;
      p = basename(fn) ;
      len = strlen(p) ;
      memcpy(lname + scdirlen + 1 + prefixlen, p, len + 1) ;
      rpsa.len = 0 ;
      lstart = lnames.len ;
      if (sarealpath(&rpsa, servicedirs[i]) < 0 || !stralloc_0(&rpsa)) goto err ;
      if (!stralloc_catb(&lnames, p, len + 1)) goto err ;
      if (symlink(rpsa.s, lname) < 0) goto errl ;
    }
    stralloc_free(&rpsa) ;
    r = s6_svc_writectl(scdir, S6_SVSCAN_CTLDIR, "a", 1) ;
    if (r <= 0) goto errsa ;
    killopts = 3 ;
    if (ftrigr_wait_and(&a, ids, m, deadline, stamp) < 0) goto errsa ;
    ftrigr_end(&a) ;
    stralloc_free(&lnames) ;
    return m ;

   errl:
    lnames.len = lstart ;
   err:
    stralloc_free(&rpsa) ;
   errsa:
    ftrigr_end(&a) ;
    do_unlink(scdir, prefix, prefixlen, maxlen, lnames.s, lnames.len, killopts) ;
    stralloc_free(&lnames) ;
    return -1 ;
  }
}
