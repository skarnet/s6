/* ISC license. */

#include <string.h>

#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <skalibs/socket.h>
#include <skalibs/exec.h>

#define USAGE "s6-ipcclient [ -q | -Q | -v ] [ -p bindpath ] [ -l localname ] path prog..."

int main (int argc, char const *const *argv)
{
  char const *bindpath = 0 ;
  char const *localname = 0 ;
  unsigned int verbosity = 1 ;
  PROG = "s6-ipcclient" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "qQvp:l:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'q' : if (verbosity) verbosity-- ; break ;
        case 'Q' : verbosity = 1 ; break ;
        case 'v' : verbosity++ ; break ;
        case 'p' : bindpath = l.arg ; break ;
        case 'l' : localname = l.arg ; break ;
        default : strerr_dieusage(100, USAGE) ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  {
    char modif[24 + IPCPATH_MAX] = "PROTO=IPC\0IPCLOCALPATH=" ;
    size_t i = 23 ;
    int s = ipc_stream_b() ;
    if (s < 0) strerr_diefu1sys(111, "create socket") ;
    if (bindpath && (ipc_bind(s, bindpath) == -1))
      strerr_diefu2sys(111, "bind socket to ", bindpath) ;
    if (!ipc_connect(s, argv[0]))
      strerr_diefu2sys(111, "connect to ", argv[0]) ;
    if (verbosity >= 2) strerr_warn3x(PROG, ": connected to ", argv[0]) ;
    if (localname)
    {
      size_t n = strlen(localname) ;
      if (n > IPCPATH_MAX) n = IPCPATH_MAX ;
      memcpy(modif + i, localname, n) ;
      i += n ; modif[i++] = 0 ;
    }
    else
    {
      int dummy ;
      if (ipc_local(s, modif + i, IPCPATH_MAX, &dummy) < 0) modif[--i] = 0 ;
      else i += strlen(modif + i) + 1 ;
    }
    if (fd_move(6, s) < 0)
      strerr_diefu2sys(111, "set up fd ", "6") ;
    if (fd_copy(7, 6) < 0)
      strerr_diefu2sys(111, "set up fd ", "7") ;
    xmexec_n(argv+1, modif, i, 2) ;
  }
}
