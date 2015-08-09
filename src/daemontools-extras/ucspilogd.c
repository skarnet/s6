/* ISC license. */

#undef INTERNAL_MARK
#ifndef SYSLOG_NAMES
#define SYSLOG_NAMES
#endif

#include <skalibs/nonposix.h>
#include <stdlib.h>
#include <syslog.h>
#include <skalibs/uint.h>
#include <skalibs/sgetopt.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr2.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/skamisc.h>

#define USAGE "ucspilogd [ -D default ] [ var... ]"
#define dieusage() strerr_dieusage(100, USAGE)

static inline void die (void)
{
  strerr_diefu1sys(111, "write to stdout") ;
}


 /*
   Hack: INTERNAL_MARK is defined by all systems that
   use the CODE stuff, and not by others (Solaris).
*/

#ifndef INTERNAL_MARK

typedef struct CODE_s CODE, *CODE_ref ;
struct CODE_s
{
  char *c_name ;
  unsigned int c_val ;
} ;

static const CODE prioritynames[] =
{
  { "emerg", LOG_EMERG },
  { "alert", LOG_ALERT },
  { "crit", LOG_CRIT },
  { "err", LOG_ERR },
  { "warning", LOG_WARNING },
  { "notice", LOG_NOTICE },
  { "info", LOG_INFO },
  { "debug", LOG_DEBUG },
  { 0, -1 }
} ;

static const CODE facilitynames[] =
{
  { "kern", LOG_KERN },
  { "user", LOG_USER },
  { "mail", LOG_MAIL },
  { "news", LOG_NEWS },
  { "uucp", LOG_UUCP },
  { "daemon", LOG_DAEMON },
  { "auth", LOG_AUTH },
  { "cron", LOG_CRON },
  { "lpr", LOG_LPR },
#ifdef LOG_SYSLOG
  { "syslog", LOG_SYSLOG },
#endif
#ifdef LOG_AUDIT
  { "audit", LOG_AUDIT },
#endif
  { "local0", LOG_LOCAL0 },
  { "local1", LOG_LOCAL1 },
  { "local2", LOG_LOCAL2 },
  { "local3", LOG_LOCAL3 },
  { "local4", LOG_LOCAL4 },
  { "local5", LOG_LOCAL5 },
  { "local6", LOG_LOCAL6 },
  { "local7", LOG_LOCAL7 },
  { 0, -1 }
} ;

#define LOG_PRI(p) ((p) & LOG_PRIMASK)
#define LOG_FAC(p) (((p) & LOG_FACMASK) / (LOG_PRIMASK + 1))

#endif


static unsigned int syslog_names (char const *line)
{
  unsigned int fpr, i ;
  int fp ;
  CODE *p = facilitynames ;

  if (line[0] != '<') return 0 ;
  i = uint_scan(line+1, &fpr) ;
  if (!i || (line[i+1] != '>')) return 0 ;
  i += 2 ;
  
  fp = LOG_FAC(fpr) << 3 ;
  for (; p->c_name ; p++) if (p->c_val == fp) break ;
  if (p->c_name)
  {
    if ((buffer_puts(buffer_1, p->c_name) < 0)
     || (buffer_put(buffer_1, ".", 1) < 1)) die() ;
  }
  else
  {
    if (buffer_put(buffer_1, "unknown.", 8) < 8) die() ;
    i = 0 ;
  }

  fp = LOG_PRI(fpr) ;
  for (p = prioritynames ; p->c_name ; p++) if (p->c_val == fp) break ;
  if (p->c_name)
  {
    if ((buffer_puts(buffer_1, p->c_name) < 0)
     || (buffer_put(buffer_1, ": ", 2) < 2)) die() ;
  }
  else
  {
    if (buffer_put(buffer_1, "unknown: ", 9) < 9) die() ;
    i = 0 ;
  }
  return i ;
}


int main (int argc, char const *const *argv, char const *const *envp)
{
  char const *d = "<undefined>" ;
  PROG = "ucspilogd" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "D:", &l) ;
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
    char const *envs[argc] ;
    unsigned int i = 0 ;
    for (; i < (unsigned int)argc ; i++)
    {
      envs[i] = env_get2(envp, argv[i]) ;
      if (!envs[i]) envs[i] = d ;
    }
    for (;;)
    {
      unsigned int pos = 0 ;
      satmp.len = 0 ;
      {
        register int r = skagetlnsep(buffer_0f1, &satmp, "\n", 2) ;
        if (r < 0)
        {
          if (errno != EPIPE || !stralloc_0(&satmp))
            strerr_diefu1sys(111, "read from stdin") ;
        }
        if (!r) break ;
      }
      if (!satmp.len) continue ;
      satmp.s[satmp.len-1] = '\n' ;
      if ((satmp.s[0] == '@') && (satmp.len > 26) && (byte_chr(satmp.s, 26, ' ') == 25))
      {
        if (buffer_put(buffer_1, satmp.s, 26) < 26) die() ;
        pos += 26 ;
      }
      for (i = 0 ; i < (unsigned int)argc ; i++)
        if ((buffer_puts(buffer_1, envs[i]) < 0)
         || (buffer_put(buffer_1, ": ", 2) < 2)) die() ;
      pos += syslog_names(satmp.s + pos) ;
      if (buffer_put(buffer_1, satmp.s + pos, satmp.len - pos) < (int)(satmp.len - pos)) die() ;
    }
  }
  return 0 ;
}
