/* ISC license. */

#include <unistd.h>
#include <skalibs/bytestr.h>
#include <skalibs/uint.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>

#define USAGE "s6-setuidgid username prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  unsigned int pos ;
  PROG = "s6-setuidgid" ;
  if (argc < 3) dieusage() ;
  pos = str_chr(argv[1], ':') ;
  if (argv[1][pos])
  {
    unsigned int uid = 0, gid = 0, len = uint_scan(argv[1], &uid) ;
    if (len != pos) dieusage() ;
    if (argv[1][pos+1] && !uint0_scan(argv[1]+pos+1, &gid)) dieusage() ;
    if (gid && setgid(gid)) strerr_diefu1sys(111, "setgid") ;
    if (uid && setuid(uid)) strerr_diefu1sys(111, "setuid") ;
  }
  else if (!prot_setuidgid(argv[1]))
    strerr_diefu2sys(111, "change identity to ", argv[1]) ;
  pathexec_run(argv[2], argv+2, envp) ;
  strerr_dieexec(111, argv[2]) ;
}
