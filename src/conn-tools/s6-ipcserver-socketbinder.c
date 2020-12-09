/* ISC license. */

#include <skalibs/nonposix.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/socket.h>
#include <skalibs/exec.h>

#define USAGE "s6-ipcserver-socketbinder [ -d | -D ] [ -b backlog ] [ -M | -m ] [ -a perms ] [ -B ] path prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  unsigned int backlog = SOMAXCONN ;
  int flagreuse = 1 ;
  int flagdgram = 0 ;
  unsigned int flags = O_NONBLOCK ;
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
        case 'B' : flags = 0 ; break ;
        case 'b' : if (!uint0_scan(l.arg, &backlog)) dieusage() ; break ;
        case 'a' : if (!uint0_oscan(l.arg, &perms)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (argc < 2) dieusage() ;
  close(0) ;
  if (flagdgram ? ipc_datagram_internal(flags) : ipc_stream_internal(flags))
    strerr_diefu1sys(111, "create socket") ;

  {
    mode_t m = umask(~perms & 0777) ;
    if ((flagreuse ? ipc_bind_reuse(0, argv[0]) : ipc_bind(0, argv[0])) < 0)
      strerr_diefu2sys(111, "bind to ", argv[0]) ;
    umask(m) ;
  }
  if (backlog && ipc_listen(0, backlog) < 0)
    strerr_diefu2sys(111, "listen to ", argv[0]) ;

  xexec(argv+1) ;
}
