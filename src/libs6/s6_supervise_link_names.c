/* ISC license. */

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include <skalibs/posixplz.h>
#include <skalibs/bitarray.h>
#include <skalibs/tai.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <s6/ftrigr.h>
#include <s6/ftrigw.h>
#include <s6/supervise.h>

static inline void do_unlink (char const *scdir, char const *const *names, size_t n, uint32_t killopts)
{
  for (size_t i = 0 ; i < n ; i++)
    s6_supervise_unlink(scdir, names[i], killopts) ;
}

static uint16_t registerit (ftrigr_t *a, char *fn, size_t len, gid_t gid, uint32_t options, tain const *deadline, tain *stamp)
{
  if (options & 4)
  {
    int fd ;
    memcpy(fn + len, "/down", 6) ;
    fd = open_trunc(fn) ;
    if (fd < 0) return 0 ;
    fd_close(fd) ;
  }
  memcpy(fn + len, "/" S6_SUPERVISE_EVENTDIR, 1 + sizeof(S6_SUPERVISE_EVENTDIR)) ;
  if (!ftrigw_fifodir_make(fn, gid, options & 1)) return 0 ;
  return ftrigr_subscribe(a, fn, "s", 0, deadline, stamp) ;
}

/*
  options: bit 0: force event/ mode
           bit 1: make event/ public
           bit 2: don't start the service
           bit 3: remove down files after starting supervisors
           bit 4: allow links to relative paths
*/

int s6_supervise_link_names (char const *scdir, char const *const *servicedirs, char const *const *names, size_t n, uint32_t options, tain const *deadline, tain *stamp)
{
  size_t maxnlen = 0, maxlen = 0 ;
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
    size_t nlen = strlen(names[i]) ;
    int h ;
    char subdir[len + 5] ;
    if (nlen > maxnlen) maxnlen = nlen ;
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
    ftrigr_t a = FTRIGR_ZERO ;
    stralloc rpsa = STRALLOC_ZERO ;
    gid_t gid = options & 2 ? -1 : getegid() ;
    size_t scdirlen = strlen(scdir) ;
    unsigned int m = 0 ;
    size_t i = 0 ;
    uint32_t killopts = 0 ;
    int r ;
    uint16_t ids[ntotal] ;
    char lname[scdirlen + maxnlen + 7] ;
    char fn[maxlen + 5 + (sizeof(S6_SUPERVISE_EVENTDIR) > 5 ? sizeof(S6_SUPERVISE_EVENTDIR) : 5)] ;
    if (!ftrigr_startf(&a, deadline, stamp)) return -1 ;
    memcpy(lname, scdir, scdirlen) ;
    lname[scdirlen] = '/' ;
    for (; i < n ; i++)
    {
      char const *src = servicedirs[i] ;
      size_t len = strlen(servicedirs[i]) ;
      int h = bitarray_peek(locked, i) ;
      memcpy(fn, servicedirs[i], len) ;
      if (!h)
      {
        ids[m] = registerit(&a, fn, len, gid, options, deadline, stamp) ;
        if (!ids[m++]) goto err ;
        if (bitarray_peek(logged, i))
        {
          memcpy(fn + len, "/log", 4) ;
          ids[m] = registerit(&a, fn, len + 4, gid, options, deadline, stamp) ;
          if (!ids[m++]) goto err ;
        }
      }
      fn[len] = 0 ;
      strcpy(lname + scdirlen + 1, names[i]) ;
      if (!(options & 16) && servicedirs[i][0] != '/')
      {
        rpsa.len = 0 ;
        if (sarealpath(&rpsa, servicedirs[i]) < 0 || !stralloc_0(&rpsa)) goto err ;
        src = rpsa.s ;
      }
      if (symlink(src, lname) < 0 && (!h || errno != EEXIST)) goto err ;
    }
    stralloc_free(&rpsa) ;
    r = s6_svc_writectl(scdir, S6_SVSCAN_CTLDIR, "a", 1) ;
    if (!r) errno = ENXIO ;
    if (r <= 0) goto errsa ;
    killopts = 3 ;
    if (ftrigr_wait_and(&a, ids, m, deadline, stamp) < 0) goto errsa ;
    ftrigr_end(&a) ;
    if (options & 8)
    {
      for (size_t i = 0 ; i < n ; i++)
      {
        size_t nlen = strlen(names[i]) ;
        memcpy(lname + scdirlen + 1, names[i], nlen) ;
        memcpy(lname + scdirlen + 1 + nlen, "/down", 6) ;
        unlink_void(lname) ;
      }
    }
    return m ;

   err:
    stralloc_free(&rpsa) ;
   errsa:
    ftrigr_end(&a) ;
    do_unlink(scdir, names, i, killopts | (options & 4)) ;
  }
  return -1 ;
}
