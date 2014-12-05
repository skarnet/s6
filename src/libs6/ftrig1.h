/* ISC license. */

#ifndef FTRIG1_H
#define FTRIG1_H

#include <skalibs/stralloc.h>

#define FTRIG1_PREFIX "ftrig1"
#define FTRIG1_PREFIXLEN (sizeof FTRIG1_PREFIX - 1)

typedef struct ftrig1_s ftrig1_t, *ftrig1_t_ref ;
struct ftrig1_s
{
  int fd ;
  int fdw ;
  stralloc name ;
} ;
#define FTRIG1_ZERO { .fd = -1, .fdw = -1, .name = STRALLOC_ZERO }

extern int ftrig1_make (ftrig1_t *, char const *) ;
extern void ftrig1_free (ftrig1_t *) ;

#endif
