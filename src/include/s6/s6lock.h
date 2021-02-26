/* ISC license. */

#ifndef S6LOCK_H
#define S6LOCK_H

#include <stdint.h>
#include <skalibs/tai.h>
#include <skalibs/genalloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/textclient.h>
#include <s6/config.h>


 /* Constants */

#define S6LOCKD_PROG S6_EXTBINPREFIX "s6lockd"
#define S6LOCKD_HELPER_PROG S6_LIBEXECPREFIX "s6lockd-helper"

#define S6LOCK_BANNER1 "s6lock v1.0 (b)\n"
#define S6LOCK_BANNER1_LEN (sizeof S6LOCK_BANNER1 - 1)
#define S6LOCK_BANNER2 "s6lock v1.0 (a)\n"
#define S6LOCK_BANNER2_LEN (sizeof S6LOCK_BANNER2 - 1)


 /* The client handle */

typedef struct s6lock_s s6lock_t, *s6lock_t_ref ;
struct s6lock_s
{
  textclient_t connection ;
  genalloc list ; /* array of uint16_t */
  gensetdyn data ; /* set of char */
} ;
#define S6LOCK_ZERO { .connection = TEXTCLIENT_ZERO, .list = GENALLOC_ZERO, .data = GENSETDYN_INIT(int, 2, 0, 1) }
extern s6lock_t const s6lock_zero ;


 /* Starting and ending a session */

extern int s6lock_start (s6lock_t *, char const *, tain_t const *, tain_t *) ;
#define s6lock_start_g(a, ipcpath, deadline) s6lock_start(a, ipcpath, (deadline), &STAMP)
extern int s6lock_startf (s6lock_t *, char const *, tain_t const *, tain_t *) ;
#define s6lock_startf_g(a, lockdir, deadline) s6lock_startf(a, lockdir, (deadline), &STAMP)
extern void s6lock_end (s6lock_t *) ;
                    

 /* Asynchronous primitives */

#define s6lock_fd(a) textclient_fd(&(a)->connection)
extern int s6lock_update (s6lock_t *) ;
extern int s6lock_check (s6lock_t *, uint16_t) ;


 /* Synchronous functions */

#define S6LOCK_OPTIONS_SH 0x0000U
#define S6LOCK_OPTIONS_EX 0x0001U

extern int s6lock_acquire (s6lock_t *, uint16_t *, char const *, uint32_t, tain_t const *, tain_t const *, tain_t *) ;
#define s6lock_acquire_g(a, id, path, options, limit, deadline) s6lock_acquire(a, id, path, options, limit, (deadline), &STAMP)
#define s6lock_acquire_sh(a, id, path, limit, deadline, stamp) s6lock_aquire(a, id, path, S6LOCK_OPTIONS_SH, limit, deadline, stamp)
#define s6lock_acquire_ex(a, id, path, limit, deadline, stamp) s6lock_aquire(a, id, path, S6LOCK_OPTIONS_EX, limit, deadline, stamp)
#define s6lock_acquire_sh_g(a, id, path, limit, deadline) s6lock_acquire_sh(a, id, path, limit, (deadline), &STAMP)
#define s6lock_acquire_ex_g(a, id, path, limit, deadline) s6lock_acquire_ex(a, id, path, limit, (deadline), &STAMP)
extern int s6lock_release (s6lock_t *, uint16_t, tain_t const *, tain_t *) ;
#define s6lock_release_g(a, id, deadline) s6lock_release(a, id, (deadline), &STAMP)

extern int s6lock_wait_and (s6lock_t *, uint16_t const *, unsigned int, tain_t const *, tain_t *) ;
#define s6lock_wait_and_g(a, list, len, deadline) s6lock_wait_and(a, list, len, (deadline), &STAMP)
extern int s6lock_wait_or  (s6lock_t *, uint16_t const *, unsigned int, tain_t const *, tain_t *) ;
#define s6lock_wait_or_g(a, list, len, deadline) s6lock_wait_or(a, list, len, (deadline), &STAMP)

#endif
