/* ISC license. */

#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <skalibs/allreadwrite.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <s6/supervise.h>

static int truncit (char const *s)
{
  int fd = open_trunc(s) ;
  if (fd < 0) return -1 ;
  fd_close(fd) ;
  return 0 ;
}

ssize_t s6_dtally_read (char const *sv, s6_dtally_t *tab, size_t max)
{
  int e = errno ;
  size_t len = strlen(sv) ;
  size_t n ;
  int fd ;
  struct stat st ;
  char fn[len + sizeof(S6_DTALLY_FILENAME) + 1] ;
  memcpy(fn, sv, len) ;
  memcpy(fn + len, "/" S6_DTALLY_FILENAME, sizeof(S6_DTALLY_FILENAME) + 1) ;
  fd = open_read(fn) ;
  if (fd < 0) return errno == ENOENT ? truncit(fn) : -1 ;
  if (fstat(fd, &st) < 0) goto err ;
  if (st.st_size % S6_DTALLY_PACK)
  {
    fd_close(fd) ;
    return truncit(fn) ;
  }
  n = st.st_size / S6_DTALLY_PACK ;
  if (n > max) n = max ;
  {
    char tmp[n ? S6_DTALLY_PACK * n : 1] ;
    if (lseek(fd, -(off_t)(n * S6_DTALLY_PACK), SEEK_END) < 0) goto err ;
    errno = EPIPE ;
    if (allread(fd, tmp, n * S6_DTALLY_PACK) < n * S6_DTALLY_PACK) goto err ;
    fd_close(fd) ;
    for (size_t i = 0 ; i < n ; i++) s6_dtally_unpack(tmp + i * S6_DTALLY_PACK, tab + i) ;
  }
  errno = e ;
  return n ;

 err:
  fd_close(fd) ;
  return -1 ;
}
