/* ISC license. */

#ifndef S6_SVLISTEN_H
#define S6_SVLISTEN_H

#include <skalibs/uint16.h>
#include <skalibs/tai.h>
#include <s6/ftrigr.h>

typedef void action_func_t (void) ;
typedef action_func_t *action_func_t_ref ;

typedef struct s6_svlisten_s s6_svlisten_t, *s6_svlisten_t_ref ;
struct s6_svlisten_s
{
  ftrigr_t a ;
  unsigned int n ;
  uint16 *ids ;
  unsigned char *upstate ;
  unsigned char *readystate ;
} ;
#define S6_SVLISTEN_ZERO { .a = FTRIGR_ZERO, .n = 0, .ids = 0, .upstate = 0, .readystate = 0 }

extern void s6_svlisten_signal_handler (void) ;
extern int s6_svlisten_selfpipe_init (void) ;
extern void s6_svlisten_init (int, char const *const *, s6_svlisten_t *, uint16 *, unsigned char *, unsigned char *, tain_t const *) ;
extern int s6_svlisten_loop (s6_svlisten_t *, int, int, int, tain_t const *, int, action_func_t_ref) ;

#endif
