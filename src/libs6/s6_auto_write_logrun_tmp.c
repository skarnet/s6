/* ISC license. */

#include <string.h>

#include <skalibs/posixplz.h>
#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/skamisc.h>

#include <execline/config.h>

#include <s6/config.h>
#include <s6/auto.h>

int s6_auto_write_logrun_tmp (char const *runfile, char const *loguser, char const *logdir, unsigned int stamptype, unsigned int nfiles, uint64_t filesize, uint64_t maxsize, char const *prefix, stralloc *sa)
{
  buffer b ;
  char buf[1024] ;
  char fmt[UINT64_FMT] ;
  int fd = open_trunc(runfile) ;
  if (fd < 0) return 0 ;
  buffer_init(&b, &buffer_write, fd, buf, 1024) ;
  if (buffer_puts(&b, "#!" EXECLINE_SHEBANGPREFIX "execlineb -P\n") < 0) goto err ;
  if (loguser)
  {
    if (buffer_puts(&b, S6_EXTBINPREFIX "s6-setuidgid ") < 0
     || !string_quote(sa, loguser, strlen(loguser))
     || buffer_put(&b, sa->s, sa->len) < 0
     || buffer_put(&b, "\n", 1) < 0) goto err ;
    sa->len = 0 ;
  }
  if (buffer_puts(&b, S6_EXTBINPREFIX "s6-log -bd3 -- ") < 0
   || (stamptype & 1 && buffer_put(&b, "t ", 2) < 0)
   || (stamptype & 2 && buffer_put(&b, "T ", 2) < 0)
   || buffer_put(&b, "n", 1) < 0
   || buffer_put(&b, fmt, uint_fmt(fmt, nfiles)) < 0
   || buffer_put(&b, " s", 2) < 0
   || buffer_put(&b, fmt, uint64_fmt(fmt, filesize)) < 0
   || buffer_put(&b, " ", 1) < 0) goto err ;
  if (maxsize)
  {
    if (buffer_put(&b, "S", 1) < 0
     || buffer_put(&b, fmt, uint64_fmt(fmt, maxsize)) < 0
     || buffer_put(&b, " ", 1) < 0) goto err ;
  }
  if (prefix)
  {
    if (buffer_put(&b, "p", 1) < 0
     || buffer_puts(&b, prefix) < 0
     || buffer_put(&b, " ", 1) < 0) goto err ;
  }
  if (!string_quote(sa, logdir, strlen(logdir))
   || buffer_put(&b, sa->s, sa->len) < 0
   || buffer_put(&b, "\n", 1) < 0) goto err ;
  sa->len = 0 ;

  if (!buffer_flush(&b)) goto err ;
  fd_close(fd) ;
  return 1 ;

 err:
  fd_close(fd) ;
  unlink_void(runfile) ;
  return 0 ;
}
