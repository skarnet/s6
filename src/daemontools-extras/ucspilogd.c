/* ISC license. */

#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>

#include <skalibs/sgetopt.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr.h>
#include <skalibs/stralloc.h>
#include <skalibs/skamisc.h>

#include "lolsyslog.h"

#define USAGE "ucspilogd [ -D default ] [ var... ]"
#define dieusage() strerr_dieusage(100, USAGE)

static inline void die (void)
{
  strerr_diefu1sys(111, "write to stdout") ;
}

int main (int argc, char const *const *argv)
{
  char const *d = "<undefined>" ;
  PROG = "ucspilogd" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "D:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'D' : d = l.arg ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }

  {
    unsigned int i = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    char buf[LOLSYSLOG_STRING] ;
    char const *envs[argc] ;
    for (; i < (unsigned int)argc ; i++)
    {
      envs[i] = getenv(argv[i]) ;
      if (!envs[i]) envs[i] = d ;
    }
    for (;;)
    {
      size_t pos = 0 ;
      size_t j ;
      sa.len = 0 ;
      {
        int r = skagetlnsep(buffer_0f1, &sa, "\n", 2) ;
        if (r < 0)
        {
          if (errno != EPIPE || !stralloc_0(&sa))
            strerr_diefu1sys(111, "read from stdin") ;
        }
        if (!r) break ;
      }
      if (!sa.len) continue ;
      sa.s[sa.len-1] = 0 ;
      if ((sa.s[0] == '@') && (sa.len > 26) && (byte_chr(sa.s, 26, ' ') == 25))
      {
        if (buffer_put(buffer_1, sa.s, 26) < 26) die() ;
        pos += 26 ;
      }
      for (i = 0 ; i < (unsigned int)argc ; i++)
        if ((buffer_puts(buffer_1, envs[i]) < 0)
         || (buffer_put(buffer_1, ": ", 2) < 2)) die() ;
      j = lolsyslog_string(buf, sa.s + pos) ; pos += j ;
      if (j && buffer_puts(buffer_1, buf) < 0) die() ;
      sa.s[sa.len-1] = '\n' ;
      if (buffer_put(buffer_1, sa.s + pos, sa.len - pos) < 0) die() ;
    }
  }
  return 0 ;
}
