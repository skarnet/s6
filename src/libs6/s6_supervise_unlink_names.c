/* ISC license. */

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include <skalibs/posixplz.h>
#include <skalibs/bitarray.h>

#include <s6/ftrigr.h>
#include <s6/supervise.h>

static uint16_t registerit (ftrigr_t *a, char *fn, size_t len, tain const *deadline, tain *stamp)
{
  memcpy(fn + len, "/" S6_SUPERVISE_EVENTDIR, sizeof(S6_SUPERVISE_EVENTDIR) + 1) ;
  return ftrigr_subscribe(a, fn, "x", 0, deadline, stamp) ;
}

/*
  options: bit 0: wait for s6-supervise to exit
*/

int s6_supervise_unlink_names (char const *scdir, char const *const *names, size_t n, uint32_t options, tain const *deadline, tain *stamp)
{
  size_t scdirlen = strlen(scdir) ;
  size_t ntotal = n ;
  unsigned char locked[bitarray_div8(n)] ;
  unsigned char logged[bitarray_div8(n)] ;
  if (!n) return 0 ;
  memset(locked, 0, bitarray_div8(n)) ;
  memset(logged, 0, bitarray_div8(n)) ;

  if (options & 1) for (size_t i = 0 ; i < n ; i++)
  {
    struct stat st ;
    size_t nlen = strlen(names[i]) ;
    int h ;
    char fn[scdirlen + nlen + 6] ;
    memcpy(fn, scdir, scdirlen) ;
    fn[scdirlen] = '/' ;
    memcpy(fn + scdirlen + 1, names[i], nlen + 1) ;
    h = s6_svc_ok(fn) ;
    if (h < 0) return -1 ;
    if (h) bitarray_set(locked, i) ;
    memcpy(fn + scdirlen + 1 + nlen, "/log", 5) ;
    if (stat(fn, &st) < 0)
    {
      if (errno != ENOENT) return -1 ;
    }
    else
    {
      int r ;
      if (!S_ISDIR(st.st_mode)) return (errno = ENOTDIR, -1) ;
      r = s6_svc_ok(fn) ;
      if (r < 0) return -1 ;
      if (r != h) return (errno = EINVAL, -1) ;
      bitarray_set(logged, i) ;
      ntotal++ ;
    }
  } 

  {
    ftrigr_t a = FTRIGR_ZERO ;
    unsigned int m = 0 ;
    uint16_t ids[ntotal] ;
    if (options & 1 && !ftrigr_startf(&a, deadline, stamp)) return -1 ;
    for (size_t i = 0 ; i < n ; i++)
    {
      size_t nlen = strlen(names[i]) ;
      char fn[scdirlen + nlen + 6 + sizeof(S6_SUPERVISE_EVENTDIR)] ;
      memcpy(fn, scdir, scdirlen) ;
      fn[scdirlen] = '/' ;
      memcpy(fn + scdirlen + 1, names[i], nlen) ;
      if (options & 1 && bitarray_peek(locked, i))
      {
        ids[m] = registerit(&a, fn, scdirlen + 1 + nlen, deadline, stamp) ;
        if (ids[m]) m++ ;
        if (bitarray_peek(logged, i))
        {
          memcpy(fn + scdirlen + 1 + nlen, "/log", 4) ;
          ids[m] = registerit(&a, fn, scdirlen + 5 + nlen, deadline, stamp) ;
          if (ids[m]) m++ ;
        }
      }
      fn[scdirlen + 1 + nlen] = 0 ;
      unlink_void(fn) ;
    }
    s6_svc_writectl(scdir, S6_SVSCAN_CTLDIR, "an", 2) ;
    if (options & 1)
    {
      ftrigr_wait_and(&a, ids, m, deadline, stamp) ;
      ftrigr_end(&a) ;
    }
    return m ;
  }
}
