/* ISC license. */

#ifndef S6_SUPERVISE_H
#define S6_SUPERVISE_H

#include <stdint.h>
#include <sys/types.h>

#include <skalibs/tai.h>

#define S6_SUPERVISE_CTLDIR "supervise"
#define S6_SUPERVISE_EVENTDIR "event"
#define S6_SVSCAN_CTLDIR ".s6-svscan"
#define S6_SVSTATUS_FILENAME S6_SUPERVISE_CTLDIR "/status"
#define S6_SVSTATUS_SIZE 43
#define S6_DTALLY_FILENAME S6_SUPERVISE_CTLDIR "/death_tally"
#define S6_MAX_DEATH_TALLY 4096

extern int s6_svc_ok (char const *) ;
extern int s6_svc_write (char const *, char const *, size_t) ;
extern int s6_svc_writectl (char const *, char const *, char const *, size_t) ;

typedef struct s6_svstatus_s s6_svstatus_t, *s6_svstatus_t_ref ;
struct s6_svstatus_s
{
  tain stamp ;
  tain readystamp ;
  pid_t pid ;
  pid_t pgid ;
  int wstat ;
  unsigned int flagpaused : 1 ;
  unsigned int flagfinishing : 1 ;
  unsigned int flagwantup : 1 ;
  unsigned int flagready : 1 ;
} ;

#define S6_SVSTATUS_ZERO \
{ \
  .stamp = TAIN_ZERO, \
  .readystamp = TAIN_ZERO, \
  .pid = 0, \
  .pgid = 0, \
  .wstat = 0, \
  .flagpaused = 0, \
  .flagfinishing = 0, \
  .flagwantup = 1, \
  .flagready = 1, \
}

extern void s6_svstatus_pack (char *, s6_svstatus_t const *) ;
extern void s6_svstatus_unpack (char const *, s6_svstatus_t *) ;
extern int s6_svstatus_read (char const *, s6_svstatus_t *) ;
extern int s6_svstatus_write (char const *, s6_svstatus_t const *) ;

extern int s6_supervise_link (char const *, char const *const *, size_t, char const *, uint32_t, tain const *, tain *) ;
#define s6_supervise_link_g(scdir, servicedirs, n, prefix, options, deadline) s6_supervise_link(scdir, servicedirs, n, prefix, options, (deadline), &STAMP)
extern int s6_supervise_link_names (char const *, char const *const *, char const *const *, size_t, uint32_t, tain const *, tain *) ;
#define s6_supervise_link_names_g(scdir, servicedirs, names, n, options, deadline) s6_supervise_link_names(scdir, servicedirs, names, n, options, (deadline), &STAMP)
extern void s6_supervise_unlink (char const *, char const *, uint32_t) ;
extern int s6_supervise_unlink_names (char const *, char const *const *, size_t, uint32_t, tain const *, tain *) ;
#define s6_supervise_unlink_names_g(scdir, names, n, options, deadline) s6_supervise_unlink_names(scdir, names, n, options, (deadline), &STAMP)

typedef struct s6_dtally_s s6_dtally_t, *s6_dtally_ref ;
struct s6_dtally_s
{
  tain stamp ;
  unsigned char exitcode ;
  unsigned char sig ;
} ;
#define S6_DTALLY_ZERO { .stamp = TAIN_ZERO, .exitcode = 0, .sig = 0 }

#define S6_DTALLY_PACK (TAIN_PACK + 2)

extern void s6_dtally_pack (char *, s6_dtally_t const *) ;
extern void s6_dtally_unpack (char const *, s6_dtally_t *) ;
extern ssize_t s6_dtally_read (char const *, s6_dtally_t *, size_t) ;
extern int s6_dtally_write (char const *, s6_dtally_t const *, size_t) ;

extern void s6_instance_chdirservice (char const *s) ;  /* chdirs to s/instance */

#endif
