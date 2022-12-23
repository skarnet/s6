/* ISC license. */

#ifndef S6_AUTO_H
#define S6_AUTO_H

#include <skalibs/uint64.h>
#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>

typedef int s6_buffer_writer_func (buffer *, void *) ;
typedef s6_buffer_writer_func *s6_buffer_writer_func_ref ;

extern int s6_auto_write_logrun (char const *, char const *, char const *, unsigned int, unsigned int, uint64_t, uint64_t, char const *) ;
extern int s6_auto_write_logrun_tmp (char const *, char const *, char const *, unsigned int, unsigned int, uint64_t, uint64_t, char const *, stralloc *) ;

extern void s6_auto_write_logger (char const *, char const *, char const *, unsigned int, unsigned int, uint64_t, uint64_t, char const *, char const *, char const *) ;
extern void s6_auto_write_logger_tmp (char const *, char const *, char const *, unsigned int, unsigned int, uint64_t, uint64_t, char const *, char const *, char const *, stralloc *sa) ;

extern void s6_auto_write_service (char const *, unsigned int, s6_buffer_writer_func_ref, void *, char const *) ;

#endif
