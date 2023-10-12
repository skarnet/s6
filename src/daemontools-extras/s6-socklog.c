/* ISC license. */

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>

#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/buffer.h>
#include <skalibs/error.h>
#include <skalibs/strerr.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/socket.h>
#include <skalibs/ip46.h>
#include <skalibs/setgroups.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>

#include "lolsyslog.h"

#define USAGE "s6-socklog [ -d notif ] [ -r ] [ -U | -u uid -g gid -G gidlist ] [ -l linelen ] [ -t lameducktimeout ] [ -x socket | -i ip_port ]"
#define dieusage() strerr_dieusage(100, USAGE)

static tain lameducktto = TAIN_INFINITE_RELATIVE ;
static int cont = 1 ;

static inline void handle_signals (void)
{
  for (;;) switch (selfpipe_read())
  {
    case -1 : strerr_diefu1sys(111, "selfpipe_read()") ;
    case 0 : return ;
    case SIGTERM :
      cont = 0 ;
      tain_add_g(&lameducktto, &lameducktto) ;
      break ;
    default : break ;
  }
}

int main (int argc, char const *const *argv)
{
  iopause_fd x[3] = { { .events = IOPAUSE_READ }, { .fd = 1 } } ;
  int flagraw = 0 ;
  char const *usock = "/dev/log" ;
  unsigned int linelen = 1024 ;
  PROG = "s6-socklog" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    unsigned int notif = 0 ;
    unsigned int t = 0 ;
    uid_t uid = 0 ;
    gid_t gid = 0 ;
    gid_t gids[NGROUPS_MAX + 1] ;
    size_t gidn = (size_t)-1 ;
    ip46 ip ;
    uint16_t port = 514 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "rd:l:t:u:g:G:Ux:i:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'r' : flagraw = 1 ; break ;
        case 'd' : if (!uint0_scan(l.arg, &notif)) dieusage() ; break ;
        case 'l' : if (!uint0_scan(l.arg, &linelen)) dieusage() ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        case 'u' : if (!uid0_scan(l.arg, &uid)) dieusage() ; break ;
        case 'g' : if (!gid0_scan(l.arg, &gid)) dieusage() ; break ;
        case 'G' : if (!gid_scanlist(gids, NGROUPS_MAX, l.arg, &gidn) && *l.arg) dieusage() ; break ;
        case 'U' :
        {
          char const *x = getenv("UID") ;
          if (!x) strerr_dienotset(100, "UID") ;
          if (!uid0_scan(x, &uid)) strerr_dieinvalid(100, "UID") ;
          x = getenv("GID") ;
          if (!x) strerr_dienotset(100, "GID") ;
          if (!gid0_scan(x, &gid)) strerr_dieinvalid(100, "GID") ;
          x = getenv("GIDLIST") ;
          if (!x) strerr_dienotset(100, "GIDLIST") ;
          if (!gid_scanlist(gids, NGROUPS_MAX+1, x, &gidn) && *x)
            strerr_dieinvalid(100, "GIDLIST") ;
          break ;
        }
        case 'x' : usock = l.arg ; break ;
        case 'i' :
        {
          size_t pos = ip46_scan(l.arg, &ip) ;
          if (!pos) dieusage() ;
          if (l.arg[pos] == '_' || (!ip46_is6(&ip) && l.arg[pos] == ':'))
            if (!uint160_scan(l.arg + pos + 1, &port)) dieusage() ;
          usock = 0 ;
          break ;
        }
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;

    if (linelen < 76) linelen = 76 ;
    if (linelen > 1048576) linelen = 1048576 ;
    if (t) tain_from_millisecs(&lameducktto, t) ;
    if (notif)
    {
      if (notif < 3) strerr_dief1x(100, "notification fd must be 3 or more") ;
      if (fcntl(notif, F_GETFD) < 0) strerr_dief1sys(100, "invalid notification fd") ;
    }

    close(0) ;
    if (fcntl(1, F_GETFD) < 0) strerr_dief2sys(100, "invalid std", "out") ;
    if (fcntl(2, F_GETFD) < 0) strerr_dief2sys(100, "invalid std", "err") ;
    if (usock)
    {
      x[2].fd = ipc_datagram_nbcoe() ;
      if (x[2].fd == -1) strerr_diefu1sys(111, "create socket") ;
      if (ipc_bind_reuse_perms(x[2].fd, usock, 0777) == -1)
        strerr_diefu2sys(111, "bind socket to ", usock) ;
    }
    else
    {
      x[2].fd = socket_udp46_nbcoe(ip46_is6(&ip)) ;
      if (x[2].fd == -1) strerr_diefu1sys(111, "create socket") ;
      if (socket_bind46_reuse(x[2].fd, &ip, port) == -1)
      {
        char fmti[IP46_FMT] ;
        char fmtp[UINT16_FMT] ;
        fmti[ip46_fmt(fmti, &ip)] = 0 ;
        fmtp[uint16_fmt(fmtp, port)] = 0 ;
        strerr_diefu5sys(111, "bind socket to ", "ip ", fmti, " port ", fmtp) ;
      }
    }

    if (gidn != (size_t)-1 && setgroups_and_gid(gid ? gid : getegid(), gidn, gids) < 0)
      strerr_diefu1sys(111, "set supplementary group list") ;
    if (gid && setgid(gid) < 0)
      strerr_diefu1sys(111, "setgid") ;
    if (uid && setuid(uid) < 0)
      strerr_diefu1sys(111, "setuid") ;

    x[0].fd = selfpipe_init() ;
    if (x[0].fd == -1) strerr_diefu1sys(111, "init selfpipe") ;
    if (!sig_altignore(SIGPIPE)) strerr_diefu1sys(111, "ignore SIGPIPE") ;
    if (!selfpipe_trap(SIGTERM)) strerr_diefu1sys(111, "trap signals") ;

    tain_now_set_stopwatch_g() ;

    if (notif)
    {
      fd_write(notif, "\n", 1) ;
      fd_close(notif) ;
    }
  }

  {
    char outbuf[linelen << 2] ;
    buffer b1 = BUFFER_INIT(&buffer_write, 1, outbuf, linelen << 2) ;
    char line[linelen + 1] ;
    while (cont || buffer_len(&b1))
    {
      ssize_t r ;
      x[1].events = buffer_len(&b1) ? IOPAUSE_WRITE : 0 ;
      x[2].events = cont && buffer_available(&b1) >= linelen + 80 ? IOPAUSE_READ : 0 ;
      r = iopause_g(x, 3, cont ? &tain_infinite : &lameducktto) ;
      if (r == -1) strerr_diefu1sys(111, "iopause") ;
      if (!r) return 99 ;
      if (x[0].revents & IOPAUSE_READ) handle_signals() ;
      if (x[1].events & x[1].revents & IOPAUSE_WRITE)
        if (!buffer_flush(&b1) && !error_isagain(errno))
          strerr_diefu1sys(111, "write to stdout") ;
      if (x[2].events & x[2].revents & IOPAUSE_READ)
      {
        if (usock)
        {
          r = sanitize_read(fd_recv(x[2].fd, line, linelen + 1, 0)) ;
          if (r == -1) strerr_diefu1sys(111, "recv") ;
        }
        else
        {
          ip46 ip ;
          uint16_t port ;
          r = sanitize_read(socket_recv46(x[2].fd, line, linelen + 1, &ip, &port)) ;
          if (r == -1) strerr_diefu1sys(111, "recv") ;
          if (r)
          {
            char fmt[IP46_FMT + UINT16_FMT + 3] ;
            size_t m = ip46_fmt(fmt, &ip) ;
            fmt[m++] = '_' ;
            m += uint16_fmt(fmt, port) ;
            fmt[m++] = ':' ; fmt[m++] = ' ' ;
            buffer_putnoflush(&b1, fmt, m) ;
          }
        }
        if (r)
        {
          size_t len = r ;
          size_t pos = 0 ;
          while (r && (!line[r-1] || line[r-1] == '\n')) r-- ;
          for (size_t i = 0 ; i < r ; i++)
            if (!line[i] || line[i] == '\n') line[i] = '~' ;
          if (!flagraw)
          {
            char sbuf[LOLSYSLOG_STRING] ;
            pos = lolsyslog_string(sbuf, line) ;
            if (pos) buffer_putsnoflush(&b1, sbuf) ;
          }
          buffer_putnoflush(&b1, line + pos, r - pos) ;
          if (len == linelen+1) buffer_putnoflush(&b1, "...", 3) ;
          buffer_putnoflush(&b1, "\n", 1) ;
        }
      }
    }
  }
  return 0 ;
}
