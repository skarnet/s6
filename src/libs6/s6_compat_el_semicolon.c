/* ISC license. */

#include <s6/config.h>

#ifndef S6_USE_EXECLINE

#include <stdlib.h>

#include <skalibs/types.h>
#include <skalibs/strerr2.h>

static unsigned int el_getstrict (void)
{
  static unsigned int strict = 0 ;
  static int first = 1 ;
  if (first)
  {
    char const *x = getenv("EXECLINE_STRICT") ;
    first = 0 ;
    if (x) uint0_scan(x, &strict) ;
  }
  return strict ;
}

int s6_compat_el_semicolon (char const **argv)
{
  static unsigned int nblock = 0 ;
  int argc1 = 0 ;
  nblock++ ;
  for (;; argc1++, argv++)
  {
    char const *arg = *argv ;
    if (!arg) return argc1 + 1 ;
    if (!arg[0]) return argc1 ;
    else if (arg[0] == ' ') ++*argv ;
    else
    {
      unsigned int strict = el_getstrict() ;
      if (strict)
      {
        char fmt1[UINT_FMT] ;
        char fmt2[UINT_FMT] ;
        fmt1[uint_fmt(fmt1, nblock)] = 0 ;
        fmt2[uint_fmt(fmt2, (unsigned int)argc1)] = 0 ;
        if (strict >= 2)
          strerr_dief6x(100, "unquoted argument ", arg, " at block ", fmt1, " position ", fmt2) ;
        else
          strerr_warnw6x("unquoted argument ", arg, " at block ", fmt1, " position ", fmt2) ;
      }
    }
  }
}

#endif
