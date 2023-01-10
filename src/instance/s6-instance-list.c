/* ISC license. */

#include <errno.h>
#include <string.h>

#include <skalibs/buffer.h>
#include <skalibs/strerr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/direntry.h>

#include <s6/supervise.h>

#define USAGE "s6-instance-list service"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  DIR *dir ;
  PROG = "s6-instance-list" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) dieusage() ;

  s6_instance_chdirservice(argv[0]) ;
  dir = opendir(".") ;
  for (;;)
  {
    direntry *d ;
    errno = 0 ;
    d = readdir(dir) ;
    if (!d) break ;
    if (d->d_name[0] == '.') continue ;
    if (buffer_puts(buffer_1, d->d_name) < 0
     || buffer_put(buffer_1, "\n", 1) < 0)
      strerr_diefu1sys(111, "write to stdout") ;
  }
  if (errno) strerr_diefu3sys(111, "readdir ", argv[0], "/instance") ;
  dir_close(dir) ;
  if (!buffer_flush(buffer_1)) strerr_diefu1sys(111, "write to stdout") ;
  return 0 ;
}
