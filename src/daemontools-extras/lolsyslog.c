/* ISC license. */

#undef INTERNAL_MARK
#ifndef SYSLOG_NAMES
#define SYSLOG_NAMES
#endif

#include <skalibs/nonposix.h>

#include <string.h>
#include <syslog.h>

#include <skalibs/types.h>

#include "lolsyslog.h"

#ifndef INTERNAL_MARK

typedef struct CODE_s CODE, *CODE_ref ;
struct CODE_s
{
  char *c_name ;
  unsigned int c_val ;
} ;

#define LOG_PRI(p) ((p) & LOG_PRIMASK)
#define LOG_FAC(p) (((p) & LOG_FACMASK) / (LOG_PRIMASK + 1))

static CODE const facilitynames[] =
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

static CODE const prioritynames[] =
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

#endif

size_t lolsyslog_string (char *out, char const *in)
{
  size_t i ;
  unsigned int fpr ;
  int fp ;
  CODE const *p = facilitynames ;

  if (in[0] != '<' || !(i = uint_scan(in+1, &fpr)) || in[1+i] != '>') return 0 ;
  fp = LOG_FAC(fpr) << 3 ;
  for (; p->c_name ; p++) if (p->c_val == fp) break ;
  out = stpcpy(out, p->c_name ? p->c_name : "unknown") ;
  *out++ = '.' ;

  p = prioritynames ;
  fp = LOG_PRI(fpr) ;
  for (; p->c_name ; p++) if (p->c_val == fp) break ;
  out = stpcpy(out, p->c_name ? p->c_name : "unknown") ;
  *out++ = ':' ; *out++ = ' ' ; *out++ = 0 ;
  return i+2 ;
}
