/* ISC license. */

#ifndef S6_COMPAT_H
#define S6_COMPAT_H

#include <s6/config.h>

#ifdef S6_USE_EXECLINE

#include <execline/execline.h>
#define s6_el_semicolon(argv) el_semicolon(argv)

#else

extern int s6_compat_el_semicolon (char const **) ;
#define s6_el_semicolon(argv) s6_compat_el_semicolon(argv)

#endif

#endif
