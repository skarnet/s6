/* ISC license. */

#ifndef S6_AUTO_H
#define S6_AUTO_H

#include <skalibs/uint64.h>
#include <skalibs/stralloc.h>

int s6_auto_write_logrun (char const *, char const *, char const *, unsigned int, unsigned int, uint64_t, uint64_t, stralloc *) ;

#endif
