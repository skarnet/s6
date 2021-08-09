/* ISC license. */

#ifndef S6_SVLISTEN_H
#define S6_SVLISTEN_H

#include <stdint.h>
#include <skalibs/tai.h>
#include <s6/ftrigr.h>

typedef void action_func (void) ;
typedef action_func *action_func_ref ;

typedef struct s6_svlisten_s s6_svlisten_t, *s6_svlisten_t_ref ;
struct s6_svlisten_s
{
  ftrigr_t a ;
  unsigned int n ;
  uint16_t *ids ;
  unsigned char *upstate ;
  unsigned char *readystate ;
} ;
#define S6_SVLISTEN_ZERO { .a = FTRIGR_ZERO, .n = 0, .ids = 0, .upstate = 0, .readystate = 0 }

extern void s6_svlisten_signal_handler (void) ;
extern int s6_svlisten_selfpipe_init (void) ;
extern void s6_svlisten_init (int, char const *const *, s6_svlisten_t *, uint16_t *, unsigned char *, unsigned char *, tain const *) ;
extern int s6_svlisten_loop (s6_svlisten_t *, int, int, int, tain const *, int, action_func_ref) ;

#endif
