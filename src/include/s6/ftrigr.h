/* ISC license. */

#ifndef S6_FTRIGR_H
#define S6_FTRIGR_H

#include <sys/uio.h>
#include <stdint.h>

#include <skalibs/tai.h>
#include <skalibs/genalloc.h>
#include <skalibs/sass.h>
#include <skalibs/sassclient.h>

#define FTRIGR_REPEAT SASS_FLAG_KEEP

typedef struct ftrigr_s ftrigr, *ftrigr_ref ;
struct ftrigr_s
{
  sassclient client ;
  genalloc data ;  /* ftrigr_data */
} ;
#define FTRIGR_ZERO { .client = SASSCLIENT_ZERO, .data = GENALLOC_ZERO }

extern int ftrigr_startf (ftrigr *, tain const *, tain *) ;
#define ftrigr_startf_g(a, deadline) ftrigr_startf(a, (deadline), &STAMP)
extern int ftrigr_start (ftrigr *, unsigned int) ;
extern void ftrigr_end (ftrigr *) ;

#define ftrigr_fd(a) sassclient_fd(&(a)->client)
extern int ftrigr_update (ftrigr *) ;
extern int ftrigr_peek (ftrigr *, uint32_t, struct iovec *) ;
extern int ftrigr_peek1 (ftrigr *, uint32_t, char *) ;
extern int ftrigr_ack (ftrigr *, uint32_t) ;
extern void ftrigr_release (ftrigr *, uint32_t) ;

extern int ftrigr_subscribe (ftrigr *, uint32_t *, uint32_t, uint32_t, char const *, char const *, tain const *, tain *) ;
#define ftrigr_subscribe_g(a, id, flags, timeout, path, re, deadline) ftrigr_subscribe(a, id, flags, timeout, path, re, (deadline), &STAMP)

extern int ftrigr_unsubscribe (ftrigr *, uint32_t, tain const *, tain *) ;
#define ftrigr_unsubscribe_g(a, id, deadline) ftrigr_unsubscribe(a, id, (deadline), &STAMP)

extern int ftrigr_wait_and (ftrigr *, uint32_t const *, unsigned int, tain const *, tain *) ;
#define ftrigr_wait_and_g(a, list, len, deadline) ftrigr_wait_and(a, list, len, (deadline), &STAMP)
extern int ftrigr_wait_or  (ftrigr *, uint32_t const *, unsigned int, struct iovec *, tain const *, tain *) ;
#define ftrigr_wait_or_g(a, list, n, v, deadline) ftrigr_wait_or(a, list, n, v, (deadline), &STAMP)

#endif
