/* ISC license. */

#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <skalibs/webipc.h>

#define USAGE "s6-ipcserver-socketbinder [ -d | -D ] [ -b backlog ] [ -M | -m ] [ -a perms ] [ -B ] path prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  unsigned int backlog = SOMAXCONN ;
  int flagreuse = 1 ;
  int flagdgram = 0 ;
  int flagblocking = 0 ;
  unsigned int perms = 0777 ;
  PROG = "s6-ipcserver-socketbinder" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "DdMmBb:a:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'D' : flagreuse = 0 ; break ;
        case 'd' : flagreuse = 1 ; break ;
        case 'M' : flagdgram = 0 ; break ;
        case 'm' : flagdgram = 1 ; break ;
        case 'B' : flagblocking = 1 ; break ;
        case 'b' : if (!uint0_scan(l.arg, &backlog)) dieusage() ; break ;
        case 'a' : if (!uint0_oscan(l.arg, &perms)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (argc < 2) dieusage() ;
  close(0) ;
  if (flagdgram ? ipc_datagram_internal(flagblocking ? 0 : DJBUNIX_FLAG_NB) : ipc_stream_internal(flagblocking ? 0 : DJBUNIX_FLAG_NB))
    strerr_diefu1sys(111, "create socket") ;

  {
    mode_t m = umask(~perms & 0777) ;
    if ((flagreuse ? ipc_bind_reuse(0, argv[0]) : ipc_bind(0, argv[0])) < 0)
      strerr_diefu2sys(111, "bind to ", argv[0]) ;
    umask(m) ;
  }
  if (backlog && ipc_listen(0, backlog) < 0)
    strerr_diefu2sys(111, "listen to ", argv[0]) ;

  xpathexec_run(argv[1], argv + 1, envp) ;
}
